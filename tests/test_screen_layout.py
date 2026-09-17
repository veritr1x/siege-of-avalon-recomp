"""native/screen_layout.h fits the 1920x1080 layout to the display's shape.

The header rewrites the game's layout record and regenerates three pieces of
HUD art, and nothing else checks its arithmetic. The harness drives it with
the runtime reduced to stubs and the game and profile under a temporary
directory.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HARNESS = ROOT / "tests" / "native" / "screen_layout_harness.cpp"


@unittest.skipUnless(shutil.which("c++"), "needs a C++ compiler")
class ScreenLayoutTests(unittest.TestCase):
    def test_layout(self):
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / "screen_layout_harness"
            build = subprocess.run(
                ["c++", "-std=c++17", "-Wall", "-Wno-unused-function",
                 "-I", str(ROOT / "native"), "-I", str(ROOT / "kit" / "runtime"),
                 "-I", str(ROOT / "kit"),
                 str(HARNESS), "-o", str(exe)],
                capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            data = Path(tmp) / "data"
            (data / "game").mkdir(parents=True)
            (data / "profile").mkdir()
            run = subprocess.run([str(exe), str(data)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("0 failures", run.stdout)


if __name__ == "__main__":
    unittest.main()
