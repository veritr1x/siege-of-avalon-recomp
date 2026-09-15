# Contributing

This repository holds Siege of Avalon: Anthology's configuration, tests and
docs on top of [recomp-kit](https://github.com/veritr1x/recomp-kit), the
submodule at `kit/`. Runtime, translator, host and tooling changes go to the
kit; open an issue there before a large architecture change. Here, the
useful work is the bring-up itself: identifying the hooks and globals that
`game.toml` still marks as sentinels, curating symbols in `globals.toml`,
and keeping [docs/analysis.md](docs/analysis.md) true.

## Prerequisites

- Python 3.9 or later; create `.venv` and install `kit/requirements-dev.txt`.
- Native builds on macOS: Apple Silicon, Xcode Command Line Tools and Git.
  CMake and Ninja come from the requirements file. iPad builds need Xcode
  with the iOS SDK and a developer team.
- Listings: [Ghidra 12.1.3](https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_12.1.3_build)
  and a Java runtime compatible with it (tested with OpenJDK 26.0.1; set
  `JAVA_HOME` to the JDK directory or pass `--java-home`).
- Your own copy of Siege of Avalon: Anthology from GOG, either installed or
  as the offline installer plus [innoextract](https://constexpr.org/innoextract/)
  (`brew install innoextract`).
- The community 1.19 patch, `SoA Anthology Patch 1.19 SteamGoG-Version.zip`.

The executable must be the patch's GOG build, `SiegeGoG.exe`, in place of
`Siege.exe`, with SHA-256:

```text
645eaa1e2725a58163932a1016e6560c174754b0bdd0a2f2970b25a0fe4ba847
```

It was linked 2025-06-09 with Embarcadero Delphi. The loader refuses other
binaries because translated addresses and data layouts are tied to this
image. Do not bypass the hash to add support for another version; a second
version is a second `game.toml`.

## Prepare your game installation

The game directory must contain `Siege.exe` and its `ArtLib`, `Interface`,
`Maps` and `Movies` directories, with the 1.19 patch unpacked over them.
Either build it in `original/patched`, which is where `game.toml` expects
the game:

```sh
innoextract --extract --output-dir original/patched "/path/to/setup_siege_of_avalon_anthology_1.03.1_(46736).exe"
unzip -o "/path/to/SoA Anthology Patch 1.19 SteamGoG-Version.zip" -d original/patched
mv original/patched/SiegeGoG.exe original/patched/Siege.exe
.venv/bin/python tools/setup.py --install original/patched --link-only
```

or link an installed copy you have patched the same way (paths containing
spaces are supported when quoted):

```sh
.venv/bin/python tools/setup.py --install "/path/to/GOG Games/Siege of Avalon - Anthology" --link-only
```

Setup verifies the executable and links the installation at ignored
`original/patched/` (a copy or extraction placed there directly is accepted,
as above). It does not download the game. Then export the listings:

```sh
.venv/bin/python tools/analyze.py \
  --ghidra-home "/path/to/ghidra_12.1.3_PUBLIC" \
  --java-home "/path/to/your/jdk/Contents/Home"
```

`tools/analyze.py` imports the executable into a disposable Ghidra project,
runs Ghidra's default analyzers and exports translation inputs into ignored
`analysis/decompiled/Siege.exe-1.19` with the kit's export script; the log is
`build/analyze.log`. The kit's own `tools/setup.py` without `--link-only`
is not used here: it exports with analysis off and expects a curated
annotation set, which this executable does not have.

## Build and run

```sh
.venv/bin/python tools/build.py --regenerate --jobs 8 \
  --allow-unmodelled "Ghidra decodes padding as code"   # translate, then compile
.venv/bin/python tools/build.py --jobs 8                # afterwards
```

`--allow-unmodelled` is required for this image: its listings decode the
padding behind some functions as code, and each such instruction becomes a
trap at its own address rather than a refused build. The state of the
bring-up is in [docs/analysis.md](docs/analysis.md). `--target ios`
builds, signs and installs the iPad app (`RECOMP_IOS_TEAM` or `--team`) once
a macOS build runs. The CMake tree lives in `build/cmake/<preset>`.

## Check your change

```sh
.venv/bin/python tools/test.py             # the kit's portable suites; no game files required
.venv/bin/python -m pytest -q tests        # this repository's config tests
.venv/bin/python tools/build.py --stub     # link-only configure of this config through the kit
.venv/bin/python kit/tools/format.py       # handwritten native code style (kit sources)
```

See [docs/testing.md](docs/testing.md).

## Updating the kit

`git -C kit checkout <commit>` then commit the submodule pointer here, with a
changelog line naming what changed. Keep the pin on a kit tag when one exists.
