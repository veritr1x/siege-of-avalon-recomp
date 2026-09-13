# Changelog

## Unreleased

- New game repository for Siege of Avalon: Anthology (GOG offline installer
  1.03.1, build 46736) in the shape of majesty-recomp and populous-recomp:
  the kit as the submodule `kit/` (pinned to kit `main` 31f0f24),
  `game.toml` and `globals.toml`, thin `tools/*.py` wrappers, config tests
  and CI.
- The pinned executable is the installer's only game executable,
  `Siege.exe` (file version 1.20.2.1431, built 2021-05-01 with Embarcadero
  Delphi, SHA-256 `0c028b58…ebd5b`).
- `game.toml` carries the measured identity of the executable (image base
  `0x00800000`, entry point `0x00bfea40`, guest root, required data
  directories, iOS bundle exclusions). The Populous-shaped hooks and globals
  the kit compiles against are sentinels in the executable's unused section
  padding until the bring-up identifies them; `tests/test_game_config.py`
  enforces that and that the exclusion list keeps the executable and the
  game's data.
- `tools/analyze.py`: listing export with Ghidra's own analyzers, because the
  kit's setup expects a curated annotation set this game does not have.
- `docs/analysis.md`: the executable's import surface (543 imports, 133 of
  them shimmed by the kit, plus 62 delay-loaded ones), what a Delphi build
  needs that the kit has never served (Unicode APIs, structured exception
  handling, the TLS directory, delay-load binding, FMOD 3, a VCL window
  drawn through GDI), and the run log of the pipeline against it.
- First measurements against the listings: Ghidra exports 8,906 functions;
  the translator stops at 4 undecodable jump-table sites and then, with the
  gaps accepted, at 198 direct call targets that are not entry points and
  40 functions using `LOCK CMPXCHG`, `XADD`, `PAUSE`, `CMC`, `STMXCSR` and
  four x87 mnemonics it has no rule for. Recorded in `docs/analysis.md`.
