# Siege of Avalon Recomp

[Build & contribute](CONTRIBUTING.md) · [Port analysis](docs/analysis.md) ·
[Testing](docs/testing.md) · [Changelog](CHANGELOG.md)

A native macOS and iPad recompilation of **Siege of Avalon: Anthology**
(the 2021 GOG release of Digital Tome's Siege of Avalon with all six
chapters), in progress. Original game instructions are translated to C
ahead of time and compiled with the native host, the way
[populous-recomp](https://github.com/veritr1x/populous-recomp) and
[majesty-recomp](https://github.com/veritr1x/majesty-recomp) do it.

The runtime, translator, hosts and mod foundation are
[recomp-kit](https://github.com/veritr1x/recomp-kit), pulled in as the git
submodule `kit/`. This repository holds what is Siege of Avalon's:
`game.toml` and `globals.toml` (identity, addresses, curated symbols),
`tests/` (the config's contract with the kit), `tools/analyze.py` (this
game's listing export) and docs.

**You need your own copy of the game.** Game executables, artwork, sound,
music, maps, movies, generated game code and replacement packs are prepared
locally and are not included. See [NOTICE](NOTICE) for ownership and
dependency credits.

## Which executable

The GOG installer ships one game executable, **`Siege.exe`** (file version
1.20.2.1431), and this port pins it. It is a 2021 **Embarcadero Delphi**
build of the game's source, not the 2000 release's binary, and that shapes
the port: the kit has so far met Visual C++ games that draw straight to
DirectDraw, and this one is a Delphi VCL application that loads DirectDraw
at run time (through the bundled `SoADDraw.dll` wrapper on Windows), plays
sound through FMOD 3, and uses Unicode Windows APIs, structured exception
handling and a TLS directory throughout. The measurements are in
[docs/analysis.md](docs/analysis.md).

## Status: surveyed; nothing runs yet

The repository, configuration and tests are in place and the kit
configures and links its hosts against this config (`tools/build.py
--stub`). The executable's import surface is measured: 133 of its 543
imports have kit shims, and the missing ones are mostly the Unicode (`W`)
variants of APIs the kit serves as `A`, plus the VCL's user32, GDI and
comctl32 layer. Before a menu can appear the kit needs structured exception
handling (the Delphi runtime raises and unwinds exceptions as ordinary
control flow; the kit aborts on `RaiseException`), TLS directory support,
a `W` shim layer, a GDI text-and-DIB set, and an `fmod.dll` shim module.
[docs/analysis.md](docs/analysis.md) lists them and keeps the run log.

## Build on macOS

The steps are the kit's. The submodule is pinned to the kit's `main`.

```sh
git clone --recurse-submodules https://github.com/veritr1x/siege-of-avalon-recomp.git
cd siege-of-avalon-recomp
python3 -m venv .venv
.venv/bin/python -m pip install -r kit/requirements-dev.txt
innoextract --extract --output-dir original/gog "/path/to/setup_siege_of_avalon_anthology_1.03.1_(46736).exe"
.venv/bin/python tools/setup.py --install original/gog --link-only
.venv/bin/python tools/analyze.py --ghidra-home /path/to/ghidra_12.1.3_PUBLIC
.venv/bin/python tools/build.py --regenerate
```

`original/gog` is where `game.toml` expects the game; extracting the GOG
installer there with [innoextract](https://constexpr.org/innoextract/) is
the same as linking an installed copy with `tools/setup.py --install
/path/to/installed/game --link-only`. `tools/setup.py`, `tools/build.py`,
`tools/test.py` and `tools/ios_logs.py` are four-line wrappers around the
kit's tools; every option is the kit's (`--help` lists them).
`tools/analyze.py` is this game's own: the kit's setup exports listings from
a curated annotation set, and none exists for this executable, so this
script runs Ghidra's analyzers instead. Outputs (the translation, the apps,
the logs) live under ignored `build/`; the game lives in ignored
`original/` and the Ghidra listings in ignored `analysis/`.

## Play on an iPad

Not yet: nothing translates. When it does, the steps are the kit's:

```sh
export RECOMP_IOS_TEAM=<your team id>       # security find-identity -v -p codesigning
.venv/bin/python tools/build.py --target ios --console
```

The app will carry the game minus `[bundle].exclude` in `game.toml` (GOG's
installer support files and every Windows DLL the game ships) and seed it
into its own Documents on first launch. The kit's touch gestures and
on-screen keypad apply as in the other ports.

## Check a change

```sh
.venv/bin/python tools/test.py              # the kit's portable suites
.venv/bin/python -m pytest -q tests         # this game's config
.venv/bin/python tools/build.py --stub      # the kit configures against this config, no game code
```

Changes to the runtime, hosts or tools belong in the kit's repository; bump
the submodule here once they land.
