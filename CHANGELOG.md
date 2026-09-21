# Changelog

## Unreleased

- Refresh the fullscreen DirectDraw window after movie mode changes through
  the updated shared runtime.

- Update the shared runtime with iPad touch-coordinate, stick-motion and frame-color
  fixes, plus regression coverage for simultaneous keys and mapped pad controls.

- Keep the tablet keyboard and KEYS tabs above the system gesture area;
  preserve the gamepad when switching from a collapsed keyboard.

- Hide the on-screen keyboard HIDE/KEYS tabs when a hardware keyboard or
  controller auto-hides the controls; retain the layout switch and saved visibility.

- Builds without `--allow-unmodelled`. The kit now judges string literals in
  `.text` before it translates anything and drops the listed bytes behind a
  call that never returns, so the mutex name behind `_Halt0` and the other
  literals the listings decode as instructions no longer become traps (3,405
  before, none now). README, CONTRIBUTING and AGENTS drop the switch from
  their build commands.

- Kit updated: it brings on-screen controls - a PlayStation-style pad beside
  the split keyboard, JSON layouts, physical controllers, phone layouts with
  portrait, and an on-device layout editor. This port maps the pad to the keys
  and clicks Siege actually reads (`TKeyEvent.FormKeyDown` and
  `AniView1MouseDown`): both sticks point, Cross walks and attacks, Circle
  casts, R2 is the force-attack Shift, Square toggles combat, Triangle and L1
  open the inventory and the character sheet, R1 the spell bar, L2 the overlay
  map, the stick clicks the journal and x-ray, Select quick-saves and Start
  closes the open dialog or opens the main menu. `[controls]` in `game.toml`
  holds it, with `default_layout = "pad+keys"`, because character and save
  names are typed and the spell hotkeys are the digits and F3-F12; the dpad is
  off and the potion and fast-travel keys are unbound, since the game ignores
  arrows, WASD, the wheel and those keys outside Ashes of Avalon. No
  `layouts/` of our own: the built-in tablet pad and keyboard serve. The old
  `[touch] keypad` knob and the `keypad` settings row are now `[controls]` and
  `controls`. See the README's Controls table.

- Kit updated: the Windows build decodes video (FFmpeg built with Visual
  Studio's compiler from MSYS2's make), so the intro movies are no longer off
  there; the README's Windows steps install MSYS2's make.

- The kit's launcher comes with this port: `game.toml` `[launcher]` names the
  game, the folders and the GOG id (2085372274, from the installer's
  `goggame` file) it looks for. It opens when the game is missing, with
  `--launcher`, or with Shift or Option held at start.

- The game fills displays of any shape from 4:3 to 32:9 instead of showing
  bars. `native/screen_layout.h` fits the 1920x1080 layout to the display
  when the game first applies it: the screen and map grow, the sidebar and
  bars follow their edges, their art and the menu backdrop are regenerated at
  the new size into the profile, and the spell bar's hit rows follow it. The
  overrides header is now `native/siege_native.h`, which includes it and
  `native/dxr_blend.h`; `tests/test_screen_layout.py` checks it.

- Kit updated: its CI builds on every platform again, and the GeneralUser GS
  SoundFont, which had never been committed, is in the kit, so a fresh
  checkout has music. The level no longer crashes now and then while a worker
  creates a window, so closing the game from a level works; windowed mode
  fills the window instead of showing a corner of the game; Direct3D 11
  frames are drawn on the GPU; the main menu loop no longer spins, and the
  game presents at the display's 120 Hz; the settings page lists only the
  rows `game.toml` names, and the window follows the Display setting.

- `game.toml` lists the settings rows (window, performance overlay, controls),
  turns off the kit's Populous mod hooks, and leaves the patch's other
  executables out of app bundles (about 98 MB).

- `tools/analyze.py` moves Ghidra's export to the directory `game.toml`
  names, so a fresh checkout builds without renaming it by hand.

- README, CONTRIBUTING, AGENTS and NOTICE describe the pinned executable (the
  patch's own `Siege.exe`), the platforms as they stand, and the source the
  port consulted. CI runs on Windows too, as the other ports' does.

- The conversation parchment's shadow darkens the scene instead of covering it
  in black, and alpha-dimmed rectangles are drawn. `native/dxr_blend.h`
  computes the two subtract blends DXEffects.DrawSub passes (it had copied
  them opaque) and replaces dxrFillRectColorBlend (0x00a52a34), the other
  routine that compiles its loop at run time, for FillRectAlpha and
  FillRectSub. `tests/test_dxr_blend.py` builds a harness against the header
  and checks every blend.

- Two more entry points in `game.toml`: 0x00b991c0, whose absence ended a
  play session in a SIGBUS, and 0x00c4d760, an event handler the listing
  folds into its neighbour.

- `smoke/world4.script` plays 1.19 into the level at 800x600 (it declines
  the new tutorial question and answers Corvus down a column of reply
  positions), and `smoke/world1080.script` does the same at 1920x1080 and
  walks the character. With `AltCursor=false` in the profile the game takes
  its D3D11 renderer, as the app does.

- Kit updated: speech over characters in Open Sans instead of 8x16 cells, a
  scrolled map no longer smears into vertical strips, the D3D11 present no
  longer caps the game at 16 fps, clicks reach the right half of the screen,
  Present waits for the refresh, and the Metal presenter no longer piles up
  drawables.

- The 2021 GOG build is no longer supported; the community 1.19 patch's GOG
  executable is the only one. README, CONTRIBUTING and NOTICE describe
  building `original/patched` - the GOG install with the patch unpacked over
  it and `SiegeGoG.exe` in place of `Siege.exe` - and the required
  `--allow-unmodelled` switch. The 2021 Ghidra listings are deleted;
  `original/gog` stays on disk as a backup of the base install, and nothing
  refers to it.

- The interface's alpha-drawn text and panels appear. The game compiles its
  alpha blit at run time and jumps to it, which translated code cannot
  execute, so the training-style list, the stat values and every dimmed panel
  were silently never drawn. `native/dxr_blend.h` replaces that routine.

- Re-pinned to the community 1.19 patch's GOG executable (2025-06-09) in
  `original/patched`, replacing the 2021 GOG build. New base, entry and
  sentinel padding; the DigiFX module is byte-identical and carried over.

- Task 14 complete: the smoke run reaches the main menu. DigiFX
  (`Dfx_p6s.dll`) is carried as an auxiliary guest module rather than shimmed:
  the kit maps, translates and serves a second guest image, and the game's
  sprite library runs as its own code. The capture shows all eight menu
  entries over the stone frame.

- Shutdown: after `ExitProcess` no guest worker runs guest code again and each
  one ends, so the seeded smoke run now exits 0 instead of crashing in a
  worker on torn-down state.

- Task 14b: the kit's software Direct3D 11 presenter now initializes, uploads
  the game's 16-bit surfaces and presents frames through the host display seam
  (`siege-delphi` 4f414ec). Native quad, alpha-blend and frame-file checks pass.
  The seeded smoke run reaches five presents, then crashes during DigiFX driver
  initialization; the title-screen main menu is still blocked.

- Task 14: a documented fullscreen smoke seed now disables both movies and
  requests system DirectDraw. Play preserves `Windowed=0`, but the pinned
  executable retains a Windows-7 override that is commented out in the supplied
  source. It still selects unsupported D3D11; the main-menu capture is black.

- Task 14: DirectDraw device enumeration now preserves the guest stack,
  and returning constructor helpers transfer SEH checkpoints to their callers
  (kit `siege-delphi` 000acd9). The corrupted chain and cleanup abort are
  resolved. Windows 6.1 still forces the game's unsupported D3D11 fullscreen
  path; the main-menu capture is black and does not pass acceptance.

- Task 14: imported-module handles, MUI language queries and VCL platform
  probes now allow startup to reach system DirectDraw (kit `siege-delphi`
  62636cd). DirectDraw creation and cooperative-level setup succeed, but a
  D3D11 delay-load exception exposes an invalid SEH chain link during
  constructor cleanup. No main-menu frame is produced; startup skin
  transparency and settings text remain open.

- Task 14: the game now reports Windows 6.1 SP1 (build 7601) to select
  system DirectDraw, using the kit's per-game Windows version setting
  (`siege-delphi` b186ecd). The smoke run stops earlier in Delphi's newly
  enabled MUI language initialization: kernel32 module lookup and three
  thread UI-language exports are missing. No main-menu frame is produced.

- Task 14: Media Foundation session failure now follows the game's no-video
  path, System32 directory queries use the expected guest path, and direct
  callees survive false scan boundaries (kit `siege-delphi` 55d982e). Video
  initialization is reached, but the selected DirectDraw wrapper is unserved;
  the capture shows a video-subsystem error, not the main menu. A later
  D3D11 delay-load exception exposes another constructor-cleanup failure.

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
