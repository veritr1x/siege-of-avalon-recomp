# Siege of Avalon Recomp

[Build & contribute](CONTRIBUTING.md) · [Port analysis](docs/analysis.md) ·
[Testing](docs/testing.md) · [Changelog](CHANGELOG.md)

A native macOS and iPad recompilation of **Siege of Avalon: Anthology**
(Digital Tome's Siege of Avalon with all six chapters, as updated by the
community 1.19 patch), in progress. Original game instructions are translated
to C ahead of time and compiled with the native host, the way
[populous-recomp](https://github.com/veritr1x/populous-recomp) and
[majesty-recomp](https://github.com/veritr1x/majesty-recomp) do it.

The runtime, translator, hosts and mod foundation are
[recomp-kit](https://github.com/veritr1x/recomp-kit), pulled in as the git
submodule `kit/`. This repository holds what is Siege of Avalon's:
`game.toml` and `globals.toml` (identity, addresses, curated symbols),
`native/` (the blend routines the game compiles at run time, written
natively), `tests/` (the config's contract with the kit), `smoke/` (scripted
runs), `tools/analyze.py` (this game's listing export) and docs. The kit is
public; recursive checkout needs no separate access token.

**You need your own copy of the game.** Game executables, artwork, sound,
music, maps, movies, generated game code and replacement packs are prepared
locally and are not included. See [NOTICE](NOTICE) for ownership and
dependency credits.

## Which executable

This port pins the community **1.19 patch**'s own **`Siege.exe`**
(`SoA Anthology Patch 1.19 SteamGoG-Version.zip`, linked 2026-01-31; its
launcher reads 1.19). Unpacking the patch over a GOG installation puts it in
place. It is an **Embarcadero Delphi** build of the game's source, not the
2000 release's binary, and that shapes the port: it is a VCL application that
draws through DirectDraw into a small **Direct3D 11** presenter, compiles
some of its blending loops at run time, plays movies through Media
Foundation, and uses Unicode Windows APIs, structured exception handling and
a TLS directory throughout. The patch's other executables (the Steam and
older builds, its tools) are not translated and are excluded from bundles.
The measurements are in [docs/analysis.md](docs/analysis.md).

## Status: plays on macOS and the iPad

The 1.19 image translates and runs from the launcher through character
creation, the intro movie and the first level, with music, saves and
settings. On macOS the level and its conversations run at the display's
120 Hz: Direct3D 11 frames are drawn on the GPU, and windowed and fullscreen
modes both fill the window. The kit is pinned to its `siege-delphi` branch;
commands, captures and results are in [docs/analysis.md](docs/analysis.md).

The game has three layouts (800x600, 1280x720, 1920x1080), so a display of
another shape shows the picture letterboxed; the iPad has bars above and
below. Frame rates in long play sessions are not yet recorded.

## Platform status

Status at kit `siege-delphi` `a809e03`. Build commands assume the private
game installation and Ghidra listings are prepared as described below.
Linux and Windows target current releases supported by SDL3.

| Platform | Verified status | Build command | Remaining checks |
| --- | --- | --- | --- |
| macOS 14+ | Plays by hand into the first level at 1920x1080, fullscreen and windowed; music, saves, settings, the intro movie and Exit work. The hover smoke runs at 110 new frames a second on a 120 Hz presenter; the world and exit smokes pass. | `.venv/bin/python tools/build.py --regenerate --allow-unmodelled "Ghidra decodes padding as code"` | Frame rates across a long session; `0080b3e0`, the game's outermost exception handler, is not translated. |
| iPadOS 17+ | Installs and plays on an iPad Pro, fullscreen at 1920x1080 with bars above and below. | `.venv/bin/python tools/build.py --target ios --team <TEAM_ID> --no-install` | Frame rate by hand; filling non-16:9 displays. |
| Linux | Never built or run on Linux. | `.venv/bin/python tools/build.py --regenerate --allow-unmodelled "Ghidra decodes padding as code" --jobs 8` | Native build/package, Vulkan window/driver validation, movies/audio, input, Save/Load and exit on hardware. |
| Windows | Never built or run on Windows. | `.venv\Scripts\python tools\build.py --regenerate --allow-unmodelled "Ghidra decodes padding as code" --jobs 8` | Native build/package, Vulkan validation, guest path separators, movies/audio, input, Save/Load and exit on hardware. |
| Android 10+ (Vulkan 1.1) | Never built for Android. | `.venv/bin/python tools/build.py --target android` | APK build, installation, boot, touch play, Save/Load and background/resume on a tablet. |

## Build on macOS

The steps are the kit's. Use the submodule commit pinned by this repository;
cloning with `--recurse-submodules` checks out that commit automatically.

```sh
git clone --recurse-submodules https://github.com/veritr1x/siege-of-avalon-recomp.git
cd siege-of-avalon-recomp
python3 -m venv .venv
.venv/bin/python -m pip install -r kit/requirements-dev.txt
innoextract --extract --output-dir original/patched "/path/to/setup_siege_of_avalon_anthology_1.03.1_(46736).exe"
unzip -o "/path/to/SoA Anthology Patch 1.19 SteamGoG-Version.zip" -d original/patched
.venv/bin/python tools/setup.py --install original/patched --link-only
.venv/bin/python tools/analyze.py --ghidra-home /path/to/ghidra_12.1.3_PUBLIC
.venv/bin/python tools/build.py --regenerate --allow-unmodelled "Ghidra decodes padding as code"
```

`original/patched` is where `game.toml` expects the game: the GOG install
with the 1.19 patch unpacked over it. Building it there with
[innoextract](https://constexpr.org/innoextract/) is the same as linking an
installed copy you have patched the same way with `tools/setup.py --install
/path/to/installed/game --link-only`. `--allow-unmodelled` is required: the
listings decode the padding behind some functions as code, and each such
instruction becomes a trap at its own address instead of refusing the build.
`tools/setup.py`, `tools/build.py`, `tools/test.py` and `tools/ios_logs.py`
are four-line wrappers around the kit's tools; every option is the kit's
(`--help` lists them). `tools/analyze.py` is this game's own: the kit's setup
exports listings from a curated annotation set, and none exists for this
executable, so this script runs Ghidra's analyzers instead. Outputs (the
translation, the apps, the logs) live under ignored `build/`; the game lives
in ignored `original/` and the Ghidra listings in ignored `analysis/`.

The app is `build/SiegeOfAvalonRecomp.app`. It opens the game's launcher;
**Play** starts the game. The profile (settings, saves) is the kit's; F10
opens the kit's settings page, where Display chooses windowed or fullscreen.

## Build on Linux

**Never built or run on Linux, including hardware playback.** CI covers
portable tests and a stub build without game code; it does not establish
native packaging or gameplay.

Start in a recursive checkout with your patched installation copied to
`original/patched`. Install Python with venv support, Ghidra 12.1.3 and a
compatible JDK as described in [CONTRIBUTING.md](CONTRIBUTING.md), and the
kit's native build dependencies (the Ubuntu package list is in
[majesty-recomp](https://github.com/veritr1x/majesty-recomp#build-on-linux)).
A Vulkan-capable driver is required for the app:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r kit/requirements-dev.txt
.venv/bin/python tools/setup.py --install original/patched --link-only
.venv/bin/python tools/analyze.py --ghidra-home /path/to/ghidra_12.1.3_PUBLIC
.venv/bin/python tools/build.py --regenerate --jobs 8 \
  --allow-unmodelled "Ghidra decodes padding as code"
RECOMP_EXE="$PWD/original/patched/Siege.exe" \
  build/package/SiegeOfAvalonRecomp/SiegeOfAvalonRecomp
```

After a successful app build, `tools/build.py` calls the kit's
`package_desktop.py`, which writes `build/package/SiegeOfAvalonRecomp/` and a
tarball. `RECOMP_EXE` names the original **executable**; its parent is the
game data root. The package includes no game files. Record a first Linux run
in [docs/analysis.md](docs/analysis.md).

## Build on Windows

**Never built or run on Windows, including hardware playback.** The CI
entry runs portable tests and a stub build.

Prepare the Ghidra listings with the macOS steps above and copy the private
`analysis/decompiled/Siege.exe-1.19-patch/` directory to the same ignored
path in the Windows checkout, with the patched installation in
`original\patched`. Install Python and LLVM's clang/lld, then use a Visual
Studio developer PowerShell with the Windows SDK and clang/lld on `PATH`:

```powershell
py -3 -m venv .venv
.venv\Scripts\python -m pip install -r kit\requirements-dev.txt
.venv\Scripts\python tools\setup.py --install original\patched --link-only
.venv\Scripts\python tools\build.py --regenerate --jobs 8 --allow-unmodelled "Ghidra decodes padding as code"
$env:RECOMP_EXE = (Resolve-Path 'original\patched\Siege.exe').Path
.\build\package\SiegeOfAvalonRecomp\SiegeOfAvalonRecomp.exe
```

The Visual Studio/MSVC-ABI compiler path keeps the kit's video decoding off,
so the intro movies do not play in that build. Record the GPU, driver and
results of a first run in [docs/analysis.md](docs/analysis.md).

## Play on an iPad

Requires Xcode with the iOS SDK, an Apple developer team signed in to Xcode,
a paired iPad with developer mode on, and a macOS build already regenerated.

```sh
export RECOMP_IOS_TEAM=<your team id>       # security find-identity -v -p codesigning
.venv/bin/python tools/build.py --target ios --console
```

This builds `build/ios/**/SiegeOfAvalonRecomp.app`, signs it, installs it on
the paired iPad (`--device <devicectl id>` when there are several) and
launches it, streaming its console with `--console`. The app carries the game
minus `[bundle].exclude` in `game.toml` (the installer's support files, the
Windows DLLs other than the Dfx module the runtime maps, and the patch's other
executables) and seeds it into its own Documents on first launch, about
1.7 GB, stamped with the executable's SHA-256 so a rebuilt bundle is
recognised. It runs fullscreen at the game's 1920x1080, scaled to the
display with bars above and below; the kit's pointer gestures apply (a tap
places the pointer and clicks, a long press right-clicks, the on-screen
keypad stands in for the keyboard). `tools/ios_logs.py --device <id> --game-dir .` pulls the
app's Documents back to `build/ios-pull`.

## Play on an Android tablet

**Never built for Android.** The kit's Android host and packaging are the
same as Majesty's; follow
[majesty-recomp's steps](https://github.com/veritr1x/majesty-recomp#play-on-an-android-tablet)
with `original/patched` as the game and `dev.recompkit.siege` as the
package, after the translation steps under [Build on macOS](#build-on-macos):

```sh
.venv/bin/python tools/build.py --target android
.venv/bin/python tools/build.py --target android --push-game --console
```

Record the device, GPU and what worked in [docs/analysis.md](docs/analysis.md).

## Check a change

```sh
.venv/bin/python tools/test.py              # the kit's portable suites
.venv/bin/python -m pytest -q tests         # this game's config and native blends
.venv/bin/python tools/build.py --stub      # the kit configures against this config, no game code
```

Changes to the runtime, hosts or tools belong in the kit's repository; bump
the submodule here once they land.
