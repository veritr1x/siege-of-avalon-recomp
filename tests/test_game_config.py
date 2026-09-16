"""Siege of Avalon: Anthology's game.toml renders the values the kit's hooks expect."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
KIT = ROOT / "kit"

# The zero-filled section padding after .rsrc in the pinned Siege.exe: .rsrc
# ends at 0x00d06800 and SizeOfImage ends the image at 0x00d07000. Mapped,
# never referenced by the game. Every unidentified hook and global lives in
# its last 512 bytes.
SENTINEL_LOW, SENTINEL_HIGH = 0x00A36E00, 0x00A37000


def load_module(name):
    spec = importlib.util.spec_from_file_location(name, KIT / "tools" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


game_config = load_module("game_config")
gen_game_config = load_module("gen_game_config")


class SiegeConfigTests(unittest.TestCase):
    def setUp(self):
        self.cfg = game_config.load(ROOT)
        self.header = gen_game_config.render_header(self.cfg)

    def test_identity(self):
        self.assertEqual(self.cfg["game"]["id"], "siege")
        self.assertEqual(self.cfg["game"]["executable"], "Siege.exe")
        # The community 1.19 patch's own Siege.exe, 2026-01-31, whose launcher
        # reads 1.19; its SiegeGoG.exe, pinned until 2026-09-16, read 1.17.1.
        self.assertEqual(self.cfg["game"]["sha256"],
                         "8c1fa17e17dbb6654d4fc9e81ca76394324000e34092c4ff466c2239e098fdcf")
        self.assertEqual(self.cfg["game"]["image_base"], 0x00800000)
        self.assertEqual(self.cfg["game"]["entry_point"], 0x00C79198)
        self.assertIn("#define RECOMP_IMAGE_BASE 0x00800000", self.header)
        self.assertEqual(self.cfg["game"]["heap_base"], 0x01E00000)
        self.assertIn('#define RECOMP_APP_NAME "SiegeOfAvalonRecomp"', self.header)
        self.assertIn('#define RECOMP_EXECUTABLE "Siege.exe"', self.header)
        self.assertIn('#define RECOMP_GUEST_ROOT "C:\\\\GOG Games\\\\Siege of Avalon - Anthology"', self.header)
        self.assertEqual(self.cfg["developer_exe_path"], (ROOT / "original/patched/Siege.exe").resolve())
        self.assertEqual(self.cfg["listings_path"],
                         (ROOT / "analysis/decompiled/Siege.exe-1.19-patch").resolve())

    def test_every_kit_macro_is_rendered(self):
        for macro in ("RECOMP_HOOK_FRAME_CLOCK_BEGIN", "RECOMP_HOOK_FRAME_CLOCK_WAIT",
                      "RECOMP_HOOK_FRAME_CLOCK_WAIT_CLAMP", "RECOMP_HOOK_FRAME_CLOCK_CLAMP_DEADLINE",
                      "RECOMP_HOOK_FRAME_CLOCK_WAIT_DEADLINE", "RECOMP_HOOK_CURSOR_SURFACE_PTRS_COUNT 2",
                      "RECOMP_HOOK_MOUSE_VTABLE", "RECOMP_HOOK_MOUSE_DEVICE_PTR", "RECOMP_HOOK_MOUSE_DEVICE_RIGHT",
                      "RECOMP_HOOK_CAMERA", "RECOMP_GLOBAL_SIMULATION_TURN_ADDR", "RECOMP_GLOBAL_COMMAND_FRAME_ADDR",
                      "RECOMP_GLOBAL_ENTITY_BASE_ADDR", "RECOMP_GLOBAL_ENTITY_BASE_STRIDE",
                      "RECOMP_GLOBAL_ENTITY_BASE_COUNT", "RECOMP_TOUCH_KEYPAD_HIDDEN 0"):
            self.assertIn("#define " + macro, self.header)

    def test_delphi_function_alignment(self):
        self.assertEqual(self.cfg["translate"]["function_alignment"], 4)

    def test_digifx_is_an_auxiliary_module_at_its_preferred_base(self):
        self.assertEqual(self.cfg["game"]["guest_size"], 0x10100000)
        [dfx] = self.cfg["aux_modules"]
        self.assertEqual(dfx["key"], "dfx")
        self.assertEqual(dfx["name"], "Dfx_p6s.dll")
        self.assertEqual((dfx["base"], dfx["size"], dfx["function_alignment"]), (0x10000000, 0x28000, 1))
        self.assertEqual(dfx["path"], (ROOT / "original/patched/Dfx_p6s.dll").resolve())
        self.assertEqual(dfx["listings_path"], (ROOT / "analysis/decompiled/Dfx_p6s.dll").resolve())
        self.assertIn("#define RECOMP_GUEST_SIZE 0x10100000u", self.header)
        self.assertIn('{"Dfx_p6s.dll", ', self.header)
        self.assertIn("0x10000000u, 0x00028000u}", self.header)

    def test_windows_version_selects_system_directdraw(self):
        self.assertEqual(self.cfg["game"]["windows_version"], "6.1")
        for field, value in (("MAJOR", 6), ("MINOR", 1), ("BUILD", 7601), ("PLATFORM", 2)):
            self.assertIn("#define RECOMP_WINDOWS_%s %du" % (field, value), self.header)

    def test_unidentified_addresses_stay_in_the_sentinel_padding(self):
        """Until a hook is found, it must point where the game never looks."""
        addresses = [self.cfg["translate"]["animation_counter"]]
        for value in self.cfg["hooks"].values():
            addresses += value if isinstance(value, list) else [value]
        addresses += [entry["addr"] for entry in self.cfg["globals"].values()]
        for address in addresses:
            self.assertTrue(SENTINEL_LOW <= address < SENTINEL_HIGH, hex(address))
        self.assertEqual(len(addresses), len(set(addresses)), "sentinels must not alias one another")
        self.assertEqual(self.cfg["translate"]["volatile_reads"], [])

    def test_bundle_exclusions_and_setup(self):
        for pattern in ("__redist", "app", "commonappdata", "tmp", "*.dll", "*.hashdb"):
            self.assertIn(pattern, self.cfg["bundle"]["exclude"])
        stage = load_module("stage_game_files")
        exclude = self.cfg["bundle"]["exclude"]
        # The executable, the game's data and its language files survive the list.
        self.assertFalse(stage.excluded(Path("Siege.exe"), exclude))
        self.assertFalse(stage.excluded(Path("Siege.english.ini"), exclude))
        self.assertFalse(stage.excluded(Path("ArtLib/Tiles/AllBlack.pox"), exclude))
        self.assertFalse(stage.excluded(Path("Maps/6catwalk.lvl"), exclude))
        self.assertFalse(stage.excluded(Path("Movies/SiegeOpening.wmv"), exclude))
        # Every Windows DLL the installer ships is x86 code the kit stands in for.
        for dll in ("fmod.dll", "Soundlib.dll", "SoADDraw.dll", "Galaxy.dll", "CGalaxy.dll", "Dfx_p6s.dll"):
            self.assertTrue(stage.excluded(Path(dll), exclude), dll)
        self.assertTrue(stage.excluded(Path("goggame-2085372274.hashdb"), exclude))
        self.assertEqual(self.cfg["setup"]["required_dirs"], ["ArtLib", "Interface", "Maps", "Movies"])
        self.assertNotIn("annotations_url", self.cfg["setup"])


if __name__ == "__main__":
    unittest.main()
