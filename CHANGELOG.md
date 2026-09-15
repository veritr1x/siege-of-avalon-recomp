# Changelog

## Unreleased

- Task 14: cleanup landings now keep exception dispatchers live and return
  through callback checkpoints (kit `siege-delphi` 5521cc7). The movie error
  recovers without corrupting the guest stack. The smoke profile uses the
  game's `ShowIntro=false` setting to reach graphics initialization, where a
  pruned dynamic-method target and unresolved wrapper module block the menu.
  Startup PNG transparency and settings-value text remain open.

- Task 14: constructor-helper SEH checkpoints and explicit unsupported Media
  Foundation exports advance past the missing-checkpoint abort (kit
  `siege-delphi` 2d2eafc). Exception cleanup still corrupts the guest return
  path before DirectDraw; the main menu remains unverified. Startup PNG
  transparency and settings-value text remain open.

- Task 14 resumed: smoke input reaches the startup form's Play control,
  the form keeps its full size, and the skin's delay-loaded AlphaBlend call
  executes (kit `siege-delphi` 93b80a3). Play dismisses the modal, but missing
  `mfplat.dll` exposes an SEH checkpoint failure before DirectDraw; the main
  menu remains unverified. Startup transparency and settings text remain open.

- Task 14: the startup form's version text now draws with the kit's bitmap
  font (kit `siege-delphi` 35277ef). The main-menu smoke script is recorded,
  but progression is blocked by host mouse routing to a hidden VCL window
  and its fixed 640x480 pointer clamp; the main menu has not been reached.

- The Delphi runtime initialises, the VCL message loop runs and the startup settings form paints under the headless host.
- Kit `siege-delphi` a764102 preserves pushed RET continuations, keeps
  speculative SEH bodies prunable, dispatches computed jumps within unrolled
  routines, supports per-game alignment and zero-register SEH frames, and
  interprets bounded compiler-generated guest thunks. Exact x87 integer
  copies now preserve resource strings and 64-bit records. Recovered CALLs
  push their decoded continuation, and oversized heap requests report guest
  registers and return candidates. RET now dispatches vtable-loaded methods,
  removing the oversized allocation and advancing startup into form loading.
  Omitted cleanup continuations are recovered through the next listed
  function boundary and kept in their establishing body.
  Protected entry evidence now takes precedence over speculative sweeps,
  and speculative UTF-16 string runs are rejected. Required cleanup aliases
  now survive speculative-owner pruning through their listed span owners,
  and the UTF-16 guard admits repeated `PUSH 0` prologues while rejecting
  relocated text. Relocated entry candidates outrank bare scan guesses,
  and final table coverage is rebuilt after ownership changes. Candidate
  boundaries, terminating paths, complete string headers, and cleanup
  provenance now keep speculative data from hiding methods.
  `msimg32.dll` now supplies tested gradients, alpha blending and color-key
  blits. Display enumeration accepts uninitialized DEVMODE sizes and shares
  DirectDraw's supported modes; proven jumps through popped return addresses
  bypass entry dispatch. Paint synthesis and unchanged-geometry notification
  suppression allow the startup form to paint. Headless and smoke refresh
  static GDI surfaces on the display clock; Task 13 acceptance now passes with
  600 presented frames in 14.3 seconds and host exit 0. Verbose import traces
  include return values. The magenta background and missing labels remain
  drawing defects for Task 14; the checkmark glyph is visible.

- Kit pinned to `siege-delphi` f93fdc6: the wide kernel32, user32, advapi32
  and version APIs, oleaut32, comctl32, a VCL window model, a GDI canvas
  with DIB blits and bitmap-font text, FMOD 3 and the MIDI helper as shim
  modules, an offline Galaxy, and structured exception handling for Delphi
  frames (design in the kit's `2026-09-14-seh-design.md`). Every import of
  `Siege.exe` has a shim signature; the translation grows to 38,196 entries
  with the exception landing blocks. Nothing runs yet.
- Kit pinned to `siege-delphi` b2f0146, which now carries kit `main` 86bf512:
  FFmpeg video, desktop and Android packaging, mss32 and user32/GDI shims,
  optional entry-point seeds and heap placement, table-gap override
  passthrough, guarded game-independent profile tests and consistent A/W
  disk-space queries. Regeneration still links 32,594 entries; the runtime
  suite's sole failure is the 337 imports with unknown argument counts.

- Kit pinned to `siege-delphi` 6a19ac7: the loader processes the TLS
  directory, `LoadLibrary` serves every registered shim table (so a game that
  loads DirectDraw at run time finds it), wide-string helpers and the `W`
  module API, and the kit's native test suites derive their expectations
  from the game directory instead of Populous's binary (under this game the
  runtime suite now fails only on the imports that still lack shims).
- The executable translates and compiles; `pop_headless` links against the translation (kit `siege-delphi` 63da563).
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
