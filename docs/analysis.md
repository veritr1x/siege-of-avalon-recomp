# Port analysis

What Siege of Avalon: Anthology needs from the kit, measured on 2026-09-13
from the GOG offline installer
`setup_siege_of_avalon_anthology_1.03.1_(46736).exe` (618,711,480 bytes)
extracted with innoextract 1.9. This is the `analyze` stage of the kit's
design (its section 3.2) done by hand, since the kit's `analyze` command is
milestone M2 work. Keep this file true as the bring-up moves.

## The executable

The installer's game directory holds one game executable, `Siege.exe`, and
the GOG launcher (`goggame-2085372274.info`) starts it with no arguments.
It is not the 2000 release's binary: it is a 2021 build of the game's
Delphi source (the unit names `SoAOS.Types`, `SoAOS.AI.Types`, `Vcl.*` and
`System.SysUtils` are in the image; the version resource still carries
Digital Tome's 1999-2000 copyright). That changes what the kit is asked
for: every other game the kit has met was Visual C++ 6 with a static C
runtime, and this one is a Delphi VCL application with the Delphi runtime
linked in.

| | `Siege.exe` |
| --- | --- |
| Version resource | FileDescription "Siege", FileVersion 1.20.2.1431, ProductVersion 1.0.0.0 |
| Size, link timestamp | 5,083,648 bytes, 2021-05-01 |
| Linker | 2.25 (Embarcadero Delphi), 32-bit, GUI subsystem, `DllCharacteristics` 0 (no dynamic base, no NX) |
| Image base, entry point | `0x00800000`, `0x00bfea40` (in `.itext`) |
| Relocations, TLS | `.reloc` 0x53154 bytes (the loader does not need them at the preferred base); a TLS directory: raw data `0xc39000`-`0xc39048`, index slot `0xbffc28`, callback list `0xc3a010` |
| SHA-256 | `0c028b582632129a43ea67da6040ecc5d78a14e3bba06fcd2e4071b06a9ebd5b` |

| | |
| --- | --- |
| Sections | `.text` 4.0 MB code (`0x00801000`, 0x3fbb60 bytes), `.itext` 0x1c78 (Delphi's initialization code, holding the entry point), `.data` 0xb8b8, `.bss` 0x26a08, `.idata` 0x3952 (imports), `.didata` 0x1070 (delay-load imports), `.edata` (three Delphi debugger exports), `.tls`, `.rdata`, `.reloc` 0x53154, `.rsrc` 0x77800 |
| Packing, protection | none: plain Delphi code |
| Sentinel padding | `.rsrc` ends at `0x00d06800` and `SizeOfImage` ends the image at `0x00d07000`; the 2 KB between are past every section's raw data and are zero in the kit's arena. `game.toml` parks its unidentified hooks and globals in the last 512 (`0x00d06e00`-`0x00d06fff`). |
| Settings | `siege.ini` beside the executable (`ScreenResolution`, `Windowed`, `ForceD3DFullscreen`, `SoundVolume`, `MusicVolume`, `LanguagePath`; read and written through `GetPrivateProfileStringW`/`WritePrivateProfileStringW`). The per-language `Siege.<language>.ini` files name the item and title databases and the map display names. |
| Saves, log | a `savegame` directory and `Siege.log`; the game asks `SHGetFolderPathW` for a per-user folder, so where they land is a listing question |
| Command line | `GetCommandLineW` is imported; no switch strings were found in the survey |

The kit takes the image base from `game.toml` (`RECOMP_IMAGE_BASE`), and
the stub configure links at `0x00800000`; the image ends well below the
kit's heap arena at `0x01000000`. The translator treats every executable
section as code, so `.itext` is covered along with `.text`. Two kit nits
from the survey: `tools/recomp/translate.py` writes `"image_base":
"00400000"` into its report as a literal, and the kit's loader does not
process a TLS directory (it prepares the TEB's TLS array for
`TlsAlloc`/`TlsGetValue` only), which the Delphi runtime's thread variables
need: an index written at `0xbffc28`, a slot holding a copy of the 0x48
raw bytes, and the callbacks run.

## Import surface

543 imports across 16 DLLs, plus 62 delay-loaded ones. "Shimmed" counts
the imports the kit's shim tables (`runtime/`, `dx/`) name as of kit `main`
31f0f24; the rest bind to the kit's logging trampolines, which return 0 and
pop only the return address, so a missing stdcall import drifts the guest
stack when it is reached.

| DLL | Imports | Shimmed | Missing, and what the gaps are |
| --- | --- | --- | --- |
| `kernel32` | 139 | 73 | the `W` file, module and resource API (`CreateFileW`, `FindFirstFileW`, `GetModuleHandleW`, `LoadLibraryW`, `LoadLibraryExW`, `GetModuleFileNameW`, `FindResourceW`, `LoadResource`, `LockResource`, `SizeofResource`, `EnumResourceNamesW`, `GetCommandLineW`, `GetStartupInfoW`, `GetPrivateProfileStringW`, `WritePrivateProfileStringW`, `CreateEventW`, `CreateMutexW`, `OpenMutexW`, `CreateFileMappingW`, …), the locale set (`GetThreadLocale`, `SetThreadLocale`, `EnumSystemLocalesW`, `EnumCalendarInfoW`, `GetDateFormatW`, `GetCPInfoExW`, `GetUserDefaultUILanguage`, `IsDBCSLeadByteEx`), `VirtualProtect`, `VirtualQuery`, `SwitchToThread`, `FormatMessageW`, `MulDiv`, the global atom API, `VerifyVersionInfoW`/`VerSetConditionMask`, `IsDebuggerPresent`, `OutputDebugStringW` |
| `user32` | 188 | 36 | the whole Unicode VCL window layer: `RegisterClassW`, `CreateWindowExW`, `DefWindowProcW`, `PeekMessageW`, `DispatchMessageW`, `SendMessageW`, `PostMessageW`, `CallWindowProcW`, `SetWindowLongW`, `GetWindowLongW`, `SetPropW`/`GetPropW`, menus, clipboard, scroll bars, keyboard layouts, monitors (`EnumDisplayMonitors`, `MonitorFromWindow`, `GetMonitorInfoW`, `EnumDisplaySettingsW`, `EnumDisplayDevicesW`), hooks (`SetWindowsHookExW`), `SetTimer`/`KillTimer`, `DrawTextW`, `FillRect`, `LoadCursorW`, `LoadIconW`, `CreateIcon`, `MsgWaitForMultipleObjects[Ex]`, `WaitMessage` |
| `gdi32` | 100 | 18 | the VCL canvas: fonts (`CreateFontIndirectW`, `ExtTextOutW`, `GetTextExtentPoint32W`, `GetTextMetricsW`, `EnumFontFamiliesExW`, `AddFontMemResourceEx`), DIBs (`CreateDIBitmap`, `SetDIBits`, `SetDIBitsToDevice`, `StretchDIBits`, `GetDIBColorTable`), blits (`StretchBlt`, `MaskBlt`, `SetStretchBltMode`), pens, brushes, regions, clipping, metafiles and printing (`CreateDCW`, `StartDocW`, `EndDoc`, which the game will not reach) |
| `comctl32` | 35 | 0 | image lists and flat scroll bars, VCL infrastructure |
| `oleaut32` | 15 | 0 | `Variant` and `SafeArray` support the Delphi runtime links; `VariantChangeType`, `VariantCopy`, `SysAllocStringLen`, … |
| `advapi32` | 17 | 1 | the `W` registry API (`RegOpenKeyExW`, `RegQueryValueExW`, `RegSetValueExW`, `RegCreateKeyExW`, …); the kit serves the `A` set |
| `fmod` | 18 | 0 | FMOD 3.20a (`_FSOUND_Init@12`, `_FSOUND_SetOutput@4`, `_FSOUND_SetDriver@4`, `_FSOUND_Sample_LoadWav@12`, `_FSOUND_PlaySoundAttrib@20`, `_FSOUND_Stream_OpenMpeg@8`, `_FSOUND_Stream_Play@8`, volume, pan, frequency, loop mode, close): the game's sound effects and MP3 music, see below |
| `ole32` | 9 | 3 | `OleInitialize`/`OleUninitialize`, `CoInitializeEx`, `CoTaskMemAlloc`/`CoTaskMemFree`, `IsEqualGUID` |
| `soundlib` | 5 | 0 | the game's own MIDI helper (`CreateMidi`, `OpenMidi`, `StopMidi`, `SetMidiVolume`, `FreeMidi`): a 2000 Visual C++ DLL that creates a DirectMusic object through `CoCreateInstance` |
| `winspool.drv` | 5 | 0 | printer enumeration the VCL links; never reached by the game |
| `version` | 3 | 0 | `GetFileVersionInfoW`, `GetFileVersionInfoSizeW`, `VerQueryValueW`: the game reads its own version resource (the kit serves the `A` set) |
| `winmm` | 2 | 2 | `timeSetEvent`, `timeKillEvent` |
| `netapi32` | 2 | 0 | `NetWkstaGetInfo`, `NetApiBufferFree`: the Delphi runtime's OS-version probe |
| `msvcrt` | 2 | 0 | `memcpy`, `memset` |
| `shell32`, `shfolder` | 1, 1 | 0 | `Shell_NotifyIconW`; `SHGetFolderPathW` (the per-user folder for saves) |
| `cgalaxy` | 1 | 0 | `cgGetGalaxyAPI`: the GOG Galaxy wrapper; a stub that returns null is enough |

133 of 543 imports are shimmed. The delay-loaded table (`.didata`, 62
entries) names `d3d11`, `d3dx10_41`, `d3dcompiler_47`, `Mf`/`Mfplat` (Media
Foundation), `uxtheme`, `imm32`, `dwmapi`, `Shcore`, `windowscodecs`,
`wtsapi32`, `msimg32` and a few `kernel32`/`user32`/`shell32`/`advapi32`
DPI and session functions. Delphi resolves these through its own helper
with `LoadLibrary` and `GetProcAddress` at first call, so they cost nothing
until reached; the kit's loader does not bind delay imports and does not
need to, but the `W` module API the helper calls must exist.

Two shapes dominate the missing set and neither is a design problem for
the kit, only volume: the `W` variants of APIs the kit already serves as
`A` (the Delphi runtime is Unicode throughout; `IsWindowUnicode` is
imported, so windows will be created and messaged in UTF-16), and the VCL
window and canvas layer, which the kit has never needed because its games
drew straight to DirectDraw. A `W`-to-`A` shim layer for kernel32, user32,
advapi32 and version, and a GDI text-and-DIB set (fonts rendered on the
host, DIB sections in guest memory, `BitBlt`/`StretchBlt` onto the
DirectDraw primary) are the two blocks of kit work.

## Exceptions: the wall

The Delphi runtime raises exceptions through `RaiseException` and unwinds
them through `RtlUnwind`, and Delphi programs use exceptions as ordinary
control flow (`EConvertError` on a bad `StrToInt`, `EFOpenError` on a
missing file, `Abort`). The kit's runtime has no structured exception
handling: `k_RaiseException` and `k_RtlUnwind` log and abort, and the TEB
is prepared with an empty SEH chain. That is the first blocker for this
game, and unlike the import gaps it is design work in the kit: walking the
guest's `FS:[0]` chain, calling each handler as translated code, and
unwinding the translated frames the way the game's own `RtlUnwind` calls
expect. Whether the translator's generated code can be unwound at all (its
frames are host frames) decides how this is done.

## Graphics: DirectDraw through a wrapper, in a VCL window

The game draws through DirectDraw, but not through an import: it loads a
module at run time, `soaddraw.dll` first and `\ddraw.dll` from the system
directory otherwise, and resolves `DirectDrawCreate` and the enumerators
from it (the Delphi `Winapi.DirectDraw` unit's names are in the image).
`SoADDraw.dll` is the shipped one: a 2021 Visual C++ build of a DirectDraw
compatibility wrapper (it exports `ddraw.dll`'s whole surface plus
`DDrawCompat_Detach`, carries `.detourc`/`.detourd` sections and hooks
GDI, user32 and display-mode functions), which GOG bundles so the 2000-era
DirectDraw code renders on current Windows. The kit's own DirectDraw is the
stand-in for both: the kit reports `soaddraw.dll` as missing (it serves no
module by that name), the game falls back to `ddraw.dll`, and the kit's
`LoadLibraryA` path serves that, so the surface methods the game then
calls against `dx/ddraw.cpp` are the first run report. The
`ForceD3DFullscreen` setting and the delay-loaded `d3d11`/`d3dx10`
functions suggest the 2021 build also has a Direct3D 11 presentation path;
which one runs by default is a listing question, and the DirectDraw one is
the one the kit models.

Everything around the DirectDraw surface is the VCL: the main window is a
`TForm` registered and created with the `W` API, and the menus, dialogs
(name entry, load and save, options) and text are GDI drawn into window
DCs and DIBs. A minimal VCL-shaped user32 (one top-level window, message
queue, timers, `DefWindowProcW`) and the GDI set above are what puts the
menu on screen.

`ScreenResolution` in `siege.ini` selects among the interface layouts the
game ships (`Interface/<language>/` holds `bottombar.bmp`,
`bottombarHD.bmp` and `bottombarFullHD.bmp`, so 800x600, an HD and a Full
HD layout), which makes the display resolution a setting, not a patch.

## Sound, music, video, network

- **Sound effects and music** go through **FMOD 3.20a** (`fmod.dll`, a
  UPX-packed May 2000 build): `FSOUND_Init`, WAV samples loaded from
  `ArtLib/Resources/Audio/SFX` and played with `FSOUND_PlaySoundAttrib`,
  MP3 music streamed with `FSOUND_Stream_OpenMpeg` (`exCarlibur.mp3`,
  `Level1Final.mp3` are named in the image). The kit has a mixer and MP3
  decoding (minimp3, from the DirectShow shims); an `fmod.dll` shim module
  of 18 stdcall entries over them is the audio item. **MIDI** goes through
  `Soundlib.dll`, five entries over DirectMusic; a shim that plays them
  through the kit's TinySoundFont, or accepts and ignores them, decides
  whether the MIDI tracks are heard.
- **Video**: the opening and closing movies are WMV (`Movies/`, 99 MB),
  played through Media Foundation (`MFStartup`, `MFCreateMediaSession`,
  `MFCreateSourceResolver`, `MFCreateVideoRendererActivate`, delay-loaded;
  the game's own message for a missing API is "Your computer does not
  support this Media Foundation API version"). The kit does not serve
  Media Foundation and has no WMV decoder; the game's own failure path
  skips the movie. The bundle keeps the files until that is confirmed.
- **Time**: `timeSetEvent`/`timeKillEvent` (shimmed), `GetTickCount`,
  `QueryPerformanceCounter`/`QueryPerformanceFrequency` (shimmed).
- **Network**: none in the game; `netapi32` is the runtime's version probe
  and `CGalaxy.dll` is GOG Galaxy (achievements), stubbed by returning
  null from `cgGetGalaxyAPI`.
- **Dfx libraries**: `Dfx_p5s.dll` and `Dfx_p6s.dll` (April 2000, no
  imports, one export `StartupLibrary`, a `Code` and a `STACK` section)
  are the 2000 release's processor-specific helper libraries; the game
  looks for them with the pattern `dfx_*.dll` and loads whatever it finds.
  What they do, and whether the 2021 build still calls into them, is a
  listing question; excluded from bundles as Windows x86 code either way.

## Data

| | |
| --- | --- |
| `ArtLib/` | 892 MB: `Resources/` (audio SFX as WAV, conversations, the item and title databases per language, the engine's layered character images and spell effects as GIF and POX sprites, journal pages, player portraits, sprite and static objects, transitions) and `Tiles/` (map tile sets as POX) |
| `Interface/` | 87 MB: the interface bitmaps per language (`english`, `german`, `polish`, `russian`, `spanish`), 93 files each |
| `Maps/` | 31 MB: 225 files, a `.lvl` and a `.zit` per map; the `6`-prefixed maps are the Anthology's sixth chapter |
| `Movies/` | 99 MB: `SiegeOpening.wmv`, `SiegeClosing.wmv` |
| `Siege.<language>.ini` | database paths, journal and ending page names, map display names per language |
| `__redist/`, `app/`, `commonappdata/`, `tmp/` | GOG's installer support, icon, web cache and slideshow images; excluded from bundles |

## Translation

The translator has not been run against the listings yet; see the run log.
What the survey predicts it will meet: Delphi's calling conventions
(`register` by default: EAX, EDX, ECX for the first three arguments, which
the translator's stdcall/cdecl shims do not assume anything about but the
import shims do), x87 code for Delphi's `Extended` and `Double`, exception
frames on every `try` block (`FS:[0]` pushes, which the translator sees as
data flow), virtual method tables and class references reached through
relocated pointers in `.data` (which the translator's `reloc` evidence
rule turns into entry points), and string literals as reference-counted
UTF-16 records rather than C strings.

## Run log

- **2026-09-13, repository created.** Installer extracted with innoextract
  into `original/gog` (1.1 GB). `Siege.exe` surveyed with pefile: identity,
  sections, 543 imports and 62 delay imports, version resource, embedded
  strings; the tables above are that survey. Kit shim coverage measured by
  matching the import table against the kit's shim tables at kit `main`
  31f0f24: 133 of 543. `game.toml`, `globals.toml`, the wrappers and the
  config tests written; `python -m pytest -q tests` 4 passed, the kit's
  portable suites through `tools/test.py` 84 passed and 3 skipped, and
  `tools/build.py --stub` configured and linked
  `build/stub/SiegeOfAvalonRecomp.app` at image base `0x00800000`. Listing
  export with Ghidra 12.1.3 and the translator's first run follow below.
- **2026-09-13, listings and the translator's first run.** `tools/analyze.py`
  with Ghidra 12.1.3 (OpenJDK 26.0.1) imported `Siege.exe`, ran the default
  analyzers and exported 8,906 functions, all 8,906 with pseudocode, into
  `analysis/decompiled/Siege.exe` (76 MB) in under a minute.
  `tools/build.py --regenerate` then stopped at the translator's first
  gate: 4 jump-table sites decoded no entries (`0x008b97e9`, `0x00972b58`,
  `0x0098542a`, `0x00ad6996`; each reads a value that is not a table). A
  direct `kit/tools/recomp/translate.py --allow-table-gaps` run to measure
  the gates behind it stopped at the second, after 45 s: **198 literal
  dispatch targets are not entry points** (direct calls into code Ghidra
  never made a function of; the log's first 20 name ten targets in
  `0x008059bc`-`0x00809dd0`, Delphi's `System` unit helpers, `0x00809b94`
  the most called), and **40 functions failed on instructions the
  translator has no rule for**: `LOCK CMPXCHG` (23), `XADD` (7), `PAUSE`
  (2), `CMC` (2), `STMXCSR` (1), all Delphi runtime atomics and spin
  waits, plus x87 `FCLEX`, `FLDLN2`, `FLDL2E` and `FBSTP tword ptr`, its
  `Extended` math. Both are kit work, in the translator: the atomics and
  the four x87 mnemonics are new rules, and the missing entry points want
  either a Ghidra pass that defines functions at every direct call target
  or the translator accepting them as entries itself. Nothing was compiled;
  `build/translate-probe/gen` is empty.
- **2026-09-14, executable translated and headless host linked (Task 4).**
  Kit `siege-delphi` prerequisites `3ca4993` (runtime instruction forms),
  `20f5235` (x87 forms) and `cac474c` (computed jumps) were already present.
  The first regeneration still failed on `CMPXCHG8B` and `EMMS`, leaving
  four literal transfers unresolved, in 44.30 s. Kit `74e4511` adds those
  two rules and Unicorn cases; `63da563` uses the loaded image base in
  `symbols.json` and adds it to `translate-report.json`. The nondefault-base
  driver regression covers both outputs. The CMPXCHG8B oracle case
  materializes the preceding comparison's flags with `PUSHFD`/`POPFD`:
  Unicorn otherwise corrupts lazy flags when both instructions execute in
  one block, whereas stepping them with a flag read preserves them.
  The case checks both qword comparison outcomes and preservation of the
  arithmetic flags other than ZF.

  `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8`
  exited 0 in **112.82 s wall time**, measured with `/usr/bin/time -p`
  (`build/regenerate.log`); translation itself took **45.28 s**. No further
  instruction rules, emitter fixes or missing-symbol fixes were needed.
  `build/recomp/translate-report.json` reports image base **`00800000`**,
  **30,027** functions considered, **29,200** emitted bodies and **32,594**
  entry points, including **3,394** alternate entries. The symbols include
  all **8,906** listed functions. Discovery recovered **21,121** PE blocks,
  then withdrew **827** guessed blocks whose dispatch went nowhere; it
  also rejected **3,712** candidates and logged **1,401** speculative
  recovery errors (the report retains the first 50). These discovery
  diagnostics are separate from the empty **`failures`**, **`table_gaps`**
  and **`table_sites_undecoded`** lists. The literal-target gate passed with
  no unresolved transfers and no `fn_... failed:` lines. This report schema
  has no `unsupported_instructions` or `unresolved_targets` keys; the empty
  failure lists and successful gates are the coverage evidence.

  `build/recomp/gen` holds **150 files, 108,085,915 bytes** (`du -sh`: 103M),
  including 146 function chunks and the dispatch table. The macOS build
  compiled them and linked **`build/recomp/pop_headless`**, a Mach-O arm64
  executable. `nm -gU` confirms all **32,594** translated function symbols,
  including `fn_00bfea40`. There were no compiler errors and one linker
  warning reducing `__DATA,__common` alignment from `0x8000` to `0x4000`.

  Validation: the two instruction cases failed on unhandled mnemonics
  before implementation, then `test_translate_insns.py` passed **26**;
  both image-base cases failed before the fix, then
  `test_translate_driver.py` passed **9**. `.venv/bin/python tools/test.py`
  passed **106**, skipped **3**; `.venv/bin/python -m pytest -q tests`
  passed **4**. Formatting and staged-source boundary checks passed before
  both kit commits. Logs are under `build/task4-*.log`. No native runtime
  suite or game execution was attempted; compilation and linking do not
  establish startup, menu rendering or gameplay.

- **2026-09-14, kit main merged into siege-delphi (Task K2).** Kit merge
  `b2f0146` has parents `df3606c` (Tasks 1-7 and the game-independent native
  suites) and upstream `86bf512`. It carries upstream FFmpeg video, desktop
  and Android packaging, mss32, user32/GDI shims, configured translation
  entry seeds, table-gap override passthrough and per-game heap placement.
  Android support is inherited from upstream; no Android work or validation
  was performed for this port.

  The sole content conflict was `runtime/CMakeLists.txt`. The resolution
  retains the wide kernel32/resource sources and both sides' target guards;
  the profile suite uses the first generated FIDX entry and is omitted if
  none exists. Semantic review retained the game-derived runtime and host
  expectations alongside upstream's new checks, `imports_has_dll` alongside
  `imports_argc`, TLS initialization alongside the heap-base changes, and
  computed-jump discrimination alongside configured entry seeds. No duplicate
  kernel32 shim names remain. A regression first exposed different virtual
  disk capacities from `GetDiskFreeSpaceA` and `W`; both now use one helper
  reporting upstream's 4 GB free of 8 GB. The new assertion failed before
  that resolution and passed afterwards.

  Neither `heap_base` nor `entry_points` is newly required: the defaults are
  `0x01000000` and an empty seed list. `game.toml` and its tests are unchanged,
  and the executable's pinned SHA-256 was verified. Regeneration through
  `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8`
  exited **0** and linked `build/recomp/pop_headless`. Translation took
  **44.56 s** and retained **32,594 entry points** (delta **0**), **827**
  withdrawn speculative blocks, and empty `failures`, `table_gaps` and
  `table_sites_undecoded` lists. Compared with the prior report, only elapsed
  time and the new `config: 0` provenance field changed. The existing linker
  warning about reducing `__DATA,__common` alignment remains.

  Validation, in the requested order (logs and exit-code files under
  `build/k2-*`):

  - The exact `.venv/bin/python -m pytest -q kit/tools/recomp/tests`
    command exited **3** during collection: legacy `test_translate.py`
    requires `RECOMP_GAME_DIR` and is a standalone game-backed harness.
    The portable selection, using `--ignore` for that file, its
    `test_eaxa.py` consumer and `test_translate_hooks.py` (which reads
    another game's hard-coded generated artifacts), exited **0**:
    **80 passed, 1 skipped**. This is not an all-directory pytest pass.
  - `.venv/bin/python -m pytest -q kit/tests`: exit **0**, **37 passed,
    2 skipped**.
  - `.venv/bin/python tools/test.py`: exit **0**, **145 passed, 3 skipped**.
  - `.venv/bin/python -m pytest -q tests`: exit **0**, **4 passed**.
  - The regeneration/link command above: exit **0**.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose`:
    exit **1** (CTest **8**), **672 checks, 1 failure, 1 skipped**. The sole
    failure is **337 imports with unknown argument counts**, down from
    Task 7's **345**. The skipped fixture is the image with no data imports.
  - The same recorded runner with `profile_tests --verbose`: exit **0**,
    both disabled and enabled modes pass; with `host_tests --verbose`:
    exit **0**, **3,921,103 checks, 0 failures**. This is the Task K1
    adaptation of the Task 5 runner, using the kit's test/build helpers;
    the ordinary test CLI still has no `-R` option.

  Formatting (**247** handwritten sources), the game-literal checker and
  staged-source boundary checker passed. The merge and game pointer are
  committed locally; the checkout override prohibits updating the sibling
  kit repository or pushing. Native validation was on macOS only. No game
  boot, menu, gameplay, video playback or iOS/Linux/Windows execution was
  attempted; linking and these suites do not establish that the game runs.
- **2026-09-14, Phase B: what a Delphi program needs, in the kit.** Kit
  `siege-delphi` f93fdc6 (28 commits on top of the merge with `main`
  86bf512) closes every runtime gap the survey listed, each unit-tested
  without the game. The loader processes the TLS directory (a reserved
  slot, the index written at `0xbffc28`, a block per thread; 7404c02).
  `LoadLibrary` answers from the shim registry, so a game that loads
  `ddraw.dll` at run time gets the kit's DirectDraw; `gm_wstr`/`gm_put_wstr`
  and the `W` module API (09cc5d4). The wide kernel32 surface in seven
  groups (files through the ANSI file seam, profile strings, named kernel
  objects, text and time, locale probes, process and version probes, a
  bounded PE resource reader; 045a9ba..df3606c). oleaut32, the wide
  registry and version APIs, comctl32 image lists with DIB drawing,
  winspool/netapi32/msvcrt/shfolder/shell32 (db5b87b..f048e58). The wide
  user32 API and a VCL window model: hierarchy, timers on the pinned clock,
  input state, monitors, menus, shared scroll bars, clipboard, drawing
  services (607e295..a55568e). GDI: window canvases and device contexts,
  DIB blits, shapes and regions, scaled bitmap-font text sharing
  `mods/font6x8.cpp`, wide font enumeration, presented through the display
  seam (483a29b..0978150). FMOD 3 over the mixer and the shared MP3 source,
  the `Soundlib.dll` MIDI helper (arities read off the DLL: `CreateMidi`,
  `StopMidi`, `FreeMidi` take nothing; `OpenMidi`, `SetMidiVolume` one
  argument) and an offline Galaxy (9345855). The `runtime_tests` check
  "imports have an unknown argument count" went 411 → 407 → 345 → 337 →
  251 → 105 → 25 → 0 across these tasks: every one of the 543 imports now
  has a signature.

  Structured exception handling (40d88de, fcc191c, spec
  `kit/docs/superpowers/specs/2026-09-14-seh-design.md`): the first design
  was measured wrong on three counts (the 2,258 handler stubs are outside
  every listing; 158 typed stubs are followed by a type/handler table, not
  code; Delphi's accepting routine keeps the dispatcher's stack, so ESP does
  not identify the registration) and revised: landing blocks are seeded as
  entry points with provenance `seh` and reached through the existing
  alternate-entry machinery, the checkpoint at each `MOV FS:[EAX],ESP` is
  the two-call host `setjmp` form `_setjmp` already uses, `RtlUnwind`
  records its target registration and the next computed jump lands on that
  registration's checkpoint, and `frame_leave` pops the records below the
  live stack (a fourth measurement caught the first predicate keeping the
  wrong frame after nested pops). Regeneration: **38,196 entries**
  (+5,602; 8,181 with `seh` provenance), `fn_008812e8` present,
  translation 89 s, compile 20 s, `pop_headless` links. Suites:
  `seh_tests` 113 checks, `runtime_tests`, `host_tests`, `dx_tests`,
  87 translator tests, 37 kit tests, all green; the kit's legacy hook suite
  wants another game's corpus and is excluded from the portable run.
  Nothing has been run against the game yet: that is Phase C.
