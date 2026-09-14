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

- **2026-09-14, first headless run: blocked before the message loop (Task 13).**
  Started from the existing SEH translation and headless binary at kit
  f93fdc6, as instructed. The pinned executable's SHA-256 was freshly
  verified. The host reads `RECOMP_MAX_FRAMES`, not the plan's guessed
  `RECOMP_HEADLESS_FRAMES`; import tracing is `RECOMP_LOG=2`, not
  `RECOMP_LOG_IMPORTS=1` or `RECOMP_LOG=verbose`. The first diagnostic run
  with the latter value exited 139 without import traces; the correctly
  traced rerun exited 6. LLDB located the bad read in `body_0080a6b8`.

  Findings, in order:

  1. `call to unknown target c70a7301 (ESP=0effff8c, return=0080a6fc)`:
     Delphi's unit initializer at `00bfd000` ends its listed body with
     `PUSH 0xbfd0de; RET`. The continuation at `00bfd0de` restores EBP.
     The translator returned to the host caller before that continuation,
     corrupting the initialization-table walk. Kit **89d663f**,
     `Translator: follow adjacent push-ret continuations without skipping
     epilogues`, lowers this adjacent pair on the PUSH path while leaving
     a separate entry at the shared RET intact. It preserves the guest
     stack write and resolves omitted continuations as direct branches.
     The instruction regression first failed with ESP/EBP mismatches
     against Unicorn. Two driver cases then reproduced the omitted-target
     gate failure before the resolver fix. An initial test setup reused an
     existing synthetic address and failed to compile; it was corrected
     before observing the semantic failure.

  2. Regeneration reported `fn_00bfea40 dispatches to 00bfec13, which is
     not an entry point`. Recovery reached the continuation's call to
     `0080a9f0`, then decoded following data and rejected the block.
     The listing for `0080a9f0` has a closed shutdown loop with no return
     path; its callers retain unreachable instructions, so the existing
     inference from a listing's final CALL did not identify it. Kit
     **f3e6f4c**, `Translator: stop recovery at calls to closed
     nonreturning loops`, conservatively recognizes closed control flow.
     Two synthetic driver cases failed before the fix. Return paths,
     outward and computed jumps, LOOP/JECXZ escapes, and calls inside a
     closed loop are covered. The first two regeneration attempts exited
     1; regeneration after this fix exited 0 and linked `pop_headless`.

  3. The rebuilt run reports `recomp: no block entry for indirect jump to
     0x0080e093 from 0x0080bdf7` and exits **6**. The listing
     `functions/0080bd50.asm` shows a variable-argument string helper
     ending in `POP EAX; LEA ESP,[ESP + EDX*0x4]; JMP EAX`: this is a
     computed return. `0080e093` is the caller's instruction after
     `CALL 0x0080bd50` in `functions/0080df4c.asm`, already translated
     inside that caller. There is **no fix commit**: distinguishing a
     computed return from a tail jump needs a translator continuation
     design decision. Merely adding that address to the dispatch table
     risks running the caller's continuation twice because its original
     host CALL remains pending. Implementation stopped at this boundary,
     as requested. The private synthetic oracle reproducer
     `build/task13_computed_return_test.py` fails with
     `ESP: native 0effff14 unicorn 0effff10`; it is not committed.

  Final run command (local output: `build/task13-run-03.log`):

  ```sh
  RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1 \
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames \
    build/recomp/pop_headless > build/task13-run-03.log 2>&1
  ```

  This run reaches `GetVersionExW`, the runtime's locale registry probes,
  `IsValidLocale` and `GetLocaleInfoW`. It has **0** import-trace lines for
  `PeekMessageW` or `MsgWaitForMultipleObjectsEx`, and no presented frames.
  No unhandled Delphi `RaiseException` was reached, so no exception object
  class/message was available to diagnose. No thread override was used.
  The existing mod-loader-failure notice also remains in the boot log;
  its consequence after initialization is unverified. Task 13 acceptance
  is **not met**; no window, menu, rendering or gameplay success is claimed.

  Validation from the game repository root (all logs under `build/`):

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py
    -k epilogue`: **1 failed, 26 deselected** before the first fix.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k loaded_image_base`: **2 failed, 2 passed, 7 deselected** before
    resolving omitted PUSH/RET continuations.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k noreturn_loop`: **2 failed, 11 deselected** before the second fix.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py
    kit/tools/recomp/tests/test_translate_driver.py
    kit/tools/recomp/tests/test_translate_seh.py`: **56 passed**.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py`:
    **101 passed, 1 skipped**. The three exclusions retain Task 12's
    boundary around the legacy corpus-dependent suites.
  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8`:
    final exit **0**, translation **89.74 s**, **38,197** entries; empty
    `failures`, `table_gaps`, and `table_sites_undecoded`. The existing
    linker alignment warning remains. Log: `build/task13-f2-regenerate.log`.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose`:
    exit **0**, **844 checks, 0 failures, 1 skipped**; the recorded helper
    uses the kit's configure/build/test helpers because the ordinary CLI
    still has no `-R` selector. This run is labelled `game` by CTest.
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose`:
    exit **0**, **113 checks, 0 failures**.
  - `.venv/bin/python -m pytest -q tests`: **4 passed**;
    `.venv/bin/python -m pytest -q kit/tests/test_game_literals.py`:
    **3 passed**.
  - `.venv/bin/python -m pytest -q build/task13_computed_return_test.py`:
    **1 failed**, the unresolved computed-return blocker described above.
  - Kit formatting, game-literal checking, staged source-boundary checking
    and whitespace checks passed before each kit commit. All commits are
    local; no sibling checkout, player saves or other platforms were changed.

  **2026-09-14 continuation, after the orchestrator's computed-return decision:**

  3. Finding 3 is now fixed by kit **61d5b95**, `Translator: return computed
     jumps to non-entry call continuations`. The translator records every
     translated CALL's next instruction in a sorted array and emits the
     binary-search predicate `recomp_is_call_return`. After preserving the
     existing entry dispatch, a computed jump to a non-entry CALL continuation
     sets EIP and returns to the pending host caller without popping or
     dispatching again. The private reproducer was promoted to the instruction
     suite; its harness recognizes the external `MAGIC_RET` sentinel without
     another pop. A driver regression checks direct, register and memory CALL
     continuations, their order, and the dispatch-before-return rule. Both
     regressions failed before implementation and pass after it. Regeneration
     exits **0**, translates in **88.4 s**, emits **38,197 entries** and links
     `pop_headless` (`build/task13-f3-regenerate.log`).

  4. The next run exits **5** with `[host] SIGSEGV in guest thread 1:
     EIP=0080ea1d ESP=0effcefc EBP=0effcf28`. LLDB locates the actual bad read
     at guest `0080ea3d` in `body_0080e9e0`: a UTF-16 read at `10000000`,
     outside guest memory. Tracing an earlier locale cleanup confirms another
     translator control-flow gap. `functions/0080df4c.asm` ends with
     `PUSH 0x80e0b8; LEA EAX,[EBP-0x168]; MOV EDX,3; CALL 0x0080ad20; RET`.
     The pushed continuation restores EDI, ESI, EBX, ESP and EBP before its
     own RET, but the generated first RET returns to the host caller early.
     LLDB stopped immediately before guest `0080e0b0`: ESP=`0effd6f8`,
     EBP=`0effd870`, `[ESP]=0080e0b8`, `[EBP+4]=0080e2ed`. Stepping out
     resumed the caller at `0080e2ed` with EIP=`0080e0b8`, ESP=`0effd6fc`
     and unchanged EBP; the correct epilogue would leave ESP=`0effd878`.
     Evidence: `build/task13-lldb-04.log` and `build/task13-finally-lldb.log`.
     There is **no fix commit** for finding 4. It needs a rule for RET through
     a pushed continuation across cleanup instructions/calls, including the
     shared cleanup's alternate entry. The adjacent PUSH/RET rule from finding
     1 and the computed-JMP rule from finding 3 do not cover it. Globally
     dispatching RET targets that are entries could repeat a real caller's
     continuation. Work stops at this further design boundary as instructed.

  The private regression `build/task13_finally_return_test.py` reproduces the
  missing epilogue against Unicorn: **1 failed**, `ESP: native 0efffefc unicorn
  0effff04` and `EBP: native 0efffefc unicorn af1ffe0d`. Its first setup hit the
  harness's explicit prohibition on CALLs; eliding the balanced helper call
  retains the nonadjacent PUSH/RET failure using LEA and MOV. It remains an
  ignored diagnostic artifact, not a committed failing test.

  Latest run command (unchanged switch set):

  ```sh
  RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1 \
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames \
    build/recomp/pop_headless > build/task13-run-04.log 2>&1
  ```

  This run passes the old unknown computed-jump failure, then reaches
  `GetSystemDefaultUILanguage`, `FindFirstFileW` and `LoadStringW`. It records
  **0** `PeekMessageW`/`MsgWaitForMultipleObjectsEx` calls, **0** unknown-target
  or missing-shim diagnostics, **0** `RaiseException` calls and no frame files.
  No unhandled Delphi exception object is available to print. Task 13's
  **600-frame, exit-0 acceptance remains unmet**.

  Additional validation on kit 61d5b95, from the game repository root:

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py
    -k popped`: **1 failed, 28 deselected** before the fix.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k computed_returns`: **1 failed, 21 deselected** before the fix.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py
    kit/tools/recomp/tests/test_translate_driver.py
    kit/tools/recomp/tests/test_translate_seh.py`: **58 passed**.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py`:
    **103 passed, 1 skipped**, with the same corpus exclusions as above.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose`:
    **844 checks, 0 failures, 1 skipped**, exit **0** (CTest label `game`).
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose`:
    **113 checks, 0 failures**, exit **0** (CTest label `nogame`).
  - `.venv/bin/python -m pytest -q tests`: **4 passed**;
    `.venv/bin/python -m pytest -q kit/tests/test_game_literals.py`:
    **3 passed**.
  - Formatting, game-literal, staged source-boundary and whitespace checks
    passed before the kit commit. Run logs and debugger artifacts remain
    ignored under `build/`; no generated code was edited and nothing was pushed.

  **2026-09-14 continuation, after the orchestrator's interior-RET decision:**

  4. Kit **a47962d**, `Translator: keep finally returns in their establishing
     body`, implements the approved rule. For a body containing pushed
     instruction-boundary continuations, every RET pops its target, applies
     any immediate stack adjustment, and switches to an interior label or
     returns normally through the default arm. Bodies without those targets
     retain plain RET. The adjacent-pair lowering remains for external
     continuations, preserving finding 1's omitted-epilogue tests.
     The private cleanup reproducer is now an instruction regression; a
     second oracle case covers `RET imm16`. Driver cases cover non-boundary
     and external immediates, shared alternate entries, fall-through and
     direct-jump cleanup, separately listed cleanup, recovered establishing
     functions, and an epilogue owned by a speculative prefix. These cases
     first failed, then passed after implementation. This is **unit-level
     validation of finding 4, not a successful regenerated game run**.

     The listing also omits the epilogue beyond the shared cleanup's RET.
     Recovery identifies the cleanup from the untyped SEH stub's landing
     JMP, requires a normal edge from the establishing body, and adopts the
     cleanup and the continuation pushed immediately before that edge.
     Existing independently named cleanup entries become wrappers into the
     establishing body. Adoption follows only the target's reachable suffix,
     so an unrelated speculative prefix is not copied into the real body.

     Full-corpus verification exposed issues not present in the initial
     synthetic tests. The first regeneration was interrupted (exit **254**)
     after profiling showed repeated owner-map copies; single-instruction
     decoding removed that overhead. The next translation took **102.3 s**,
     but compilation exited **1** with a duplicate `fn_0096adcb`. The SEH
     resolver was separating a cleanup after it had been adopted by a
     recovered establishing function. Recording the established ownership
     fixed that interaction, with a failing-then-passing driver regression.
     The following regeneration exited **1** because adopting an epilogue
     also adopted an invalid prefix (`fn_00b5ae10` dispatched to `00b5ade0`).
     Recovering only the reachable suffix fixed that case, again with a
     failing-then-passing regression. These corrections were formatted and
     amended into the single finding-4 kit commit above.

  5. The final regeneration still exits **1**:
     `fn_00b5add0 dispatches to 00b5ade0, which is not an entry point`.
     This is a further ownership boundary. PE recovery from `00b5add0`
     starts in data, conditionally branches to `00b5ae16`, and also has an
     invalid fall-through at `00b5ade0`. The valid branch reaches SEH setup
     and normal cleanup also reached by the separately recovered prologue at
     `00b5ae10` (`PUSH EBP; MOV EBP,ESP`). The earlier speculative block
     therefore contains the same establishing instructions. Marking its
     shared cleanup as owned promotes the speculative body to structural SEH provenance, preventing
     the existing pruning pass from discarding its invalid prefix. Promoting
     every overlapping body is incorrect; separating the cleanup again
     loses the approved single-body RET behavior. A rule is needed to assign
     ownership across overlapping recovered bodies, or retain valid alternate
     entries while excluding an invalid primary prefix. There is **no fix
     commit for finding 5**; work stops at this design boundary as instructed.

     `build/task13_overlapping_owner_test.py` is an ignored synthetic driver
     reproducer. It independently fails the same gate: `fn_00600fe0 dispatches
     to 005f0feb, which is not an entry point`, **1 failed**. No guest address
     was added to kit source and no dispatch gate was bypassed.

  No new headless run was made after these edits because regeneration did not
  produce a successful build. The existing `build/recomp/gen/` is from the
  earlier compile-failing attempt; the next implementation must regenerate,
  not merely rebuild that output. The previous `task13-run-04.log` remains the
  last actual boot result; its exit **5** and zero message-loop calls are
  historical, not validation of kit a47962d. The intended next run retains
  `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1`, with
  `RECOMP_PROFILE_DIR=build/task13-profile` and
  `RECOMP_FRAMES=build/task13-frames`. No thread override was added.
  Task 13's **600-frame, exit-0 acceptance remains unmet**. The pinned
  executable's SHA-256 was freshly verified again.

  Validation from the game repository root (local logs under `build/`):

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py
    -k Cleanup`: initial **2 failed, 1 passed, 28 deselected**.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k 'return_switch or finally_cleanup'`: initial **3 failed, 2 passed,
    22 deselected**. Expanded ownership cases subsequently reproduced the
    listed-owner, recovered-owner and speculative-epilogue failures before
    their fixes (`task13-f4-ownership-red.log`,
    `task13-f4-recovered-owner-red.log`, `task13-f4-speculative-red.log`).
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py
    kit/tools/recomp/tests/test_translate_driver.py
    kit/tools/recomp/tests/test_translate_seh.py`: **75 passed**.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py`:
    **120 passed, 1 skipped**, retaining the prior corpus exclusions.
  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8`:
    final exit **1**, log `build/task13-f4-regenerate-04.log`, finding 5 above.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose`:
    exit **0**, **844 checks, 0 failures, 1 skipped** (CTest label `game`).
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose`:
    exit **0**, **113 checks, 0 failures** (CTest label `nogame`).
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py`:
    **7 passed**.
  - `.venv/bin/python -m pytest -q build/task13_overlapping_owner_test.py`:
    **1 failed**, the unresolved design-boundary reproducer.
  - Formatting, game-literal, staged source-boundary and whitespace checks
    passed before each kit commit/amendment. Both repositories remain local;
    no generated code, game assets, run logs or diagnostic scripts are committed.

  **2026-09-14 continuation, after the orchestrator's entry-provenance decision:**

  5. Finding 5 is resolved by kit **4af5a2a**, `Translator: keep speculative
     SEH bodies prunable`. Entry protection is tracked independently of the
     instructions swept into a body: listed, config and SEH entries, and
     direct targets reached from protected entries, can protect an owner.
     Merely overlapping a frame or cleanup cannot promote a speculative
     entry. Speculative blocks starting with `00 00` (`ADD [EAX],AL`) are
     rejected as data. The overlapping-prefix reproducer is now a synthetic
     driver regression; it checks that the prefix disappears, the real body
     retains its cleanup wrapper and RET dispatch, and data-scan provenance
     is not promoted by SEH content. Config and direct-call cases also pass.

     Full-image validation exposed additional bookkeeping cases while
     implementing this rule. Compilation initially found duplicate
     `fn_00828370` definitions: a cleanup already contained in its owner still
     had an independent body. A failing driver regression now checks that
     adopting it retires the standalone definition. Two subsequent normal
     regenerations reached the unchanged 64-round convergence guard. SEH
     resolution was recreating retired epilogues and blocks whose entries
     precede a shared cleanup; failing regressions now preserve those entries
     as wrappers into their owner. The next compile found duplicate
     `fn_0096adcb` definitions. A bounded diagnostic trace showed two separate
     recoveries at that same address before adoption retired only one;
     resolution now reuses an existing body at the entry. These corrections
     were formatted and amended into the single finding-5 commit above.
     Three bounded diagnostic translations intentionally stopped before
     emission; their temporary tracing is absent from committed source.

     The final normal regeneration and headless link **exit 0**
     (`build/task13-f5-regenerate-05.log`). Translation took **124.9 s**:
     **29,816/30,495 functions**, **38,126 entry points**, **4,028 candidates
     rejected as data**, 151 chunks. The linker retained its existing section
     alignment warning. This is the first successfully rebuilt headless
     binary containing both findings 4 and 5.

  6. The fresh boot **exits 6** at
     `no block entry for indirect jump to 0x00807e2a from 0x00807e1a`
     (`build/task13-run-05.log`). It gets beyond the earlier locale cleanup
     corruption and reaches another `GetLocaleInfoW` call. The verified
     listing `functions/00807dc0.asm` is an unrolled fill routine: its short
     path masks the count with `AND EDX,0xfffffffe`, negates it, forms
     `EDX*2 + 0x00807e5a`, and executes `JMP EDX`. The resulting target is an
     instruction inside the same body, not a table entry or a CALL return.
     The emitter recognizes no table for this register jump and emits an
     external `recomp_jump`, so the internal stores are skipped or rejected.

     The ignored `build/task13_computed_fill_test.py` reproduces this with
     synthetic addresses and a six-byte fill. After correcting its initial
     listing grammar, the native/Unicorn comparison **fails** with
     `scratch differs first at 0e100000: native 00 unicorn a5`.
     There is **no fix commit for finding 6**. This needs a translator design
     rule for computed instruction addresses: recover bounded arithmetic
     targets into the existing local switch, or support a broader local
     dispatch rule. Either changes target discovery beyond the current
     memory-table model; work stops for that decision as instructed.

     Two nonfatal lookups remain open:
     `GetProcAddress(?, "GetLogicalProcessorInformation") -> 0 (no shim registered)`
     and `GetProcAddress(?, "RtlCompareUnicodeString") -> 0 (no shim registered)`.
     The listings and PE strings name `kernel32.dll` and `NTDLL.DLL`.
     The `?` also matters: `GetProcAddress` has no registered module for the
     supplied handle, so adding export shims alone will not repair these
     lookups. The guest proceeds past both. Module-handle behavior and the
     missing exports remain to be handled after the design boundary; neither
     is claimed fixed. No unhandled Delphi exception occurred in this boot,
     so there was no exception object to decode.

  The exact boot command was:
  `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
  RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
  build/recomp/pop_headless > build/task13-run-05.log 2>&1`.
  The host reports a **600-frame / 180-second** cap; no thread override was
  used. `grep -c 'PeekMessageW\|MsgWaitForMultipleObjectsEx'
  build/task13-run-05.log` prints **0**, and there are **0 frame files**.
  Task 13's **600-frame, exit-0 acceptance remains unmet**. The executable's
  pinned SHA-256 was freshly verified again.

  Validation from the game repository root:

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k finally_cleanup`: initial **16 failed, 4 passed, 25 deselected**
    (`task13-f5-red.log`). The duplicate-body and convergence regressions
    also failed before their fixes (`task13-f5-duplicate-red.log`,
    `task13-f5-convergence-red-final.log`, `task13-f5-prefix-red.log`).
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py`:
    final **140 passed, 1 skipped** (`task13-f5-portable-05.log`), retaining
    the prior corpus exclusions.
  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8`:
    final **exit 0** (`task13-f5-regenerate-05.log`); earlier normal attempts
    **exit 1** as described above. All build output is under `build/`.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose`:
    **844 checks, 0 failures, 1 skipped**, exit **0**, label `game`
    (`task13-f5-runtime.log`).
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose`:
    **113 checks, 0 failures**, exit **0**, label `nogame`
    (`task13-f5-seh.log`). The existing helper uses the kit build/test wrappers;
    the current `tools/test.py` CLI has no `-R` option.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py`:
    **7 passed** (`task13-f5-config-final.log`).
  - `.venv/bin/python -m pytest -q build/task13_computed_fill_test.py`:
    **1 failed**, the unresolved finding-6 reproducer
    (`task13-f6-computed-fill-red-final.log`).
  - Formatting, staged source-boundary, game-literal and whitespace checks
    passed before every kit commit/amendment. No generated code, assets,
    run logs or private diagnostic scripts are committed, and nothing is pushed.


  **2026-09-14 continuation, after the orchestrator's local computed-jump decision:**

  6. Finding 6 is resolved by kit **a8d0a94**, `Translator: dispatch computed
     jumps within their own body`. A table-less indirect JMP first checks
     the current body's range and switches over every instruction boundary;
     a matching address stays in the same host frame through its local
     label. Addresses outside the body, and holes or instruction interiors,
     retain the existing `recomp_jump` entry/call-return/unknown path. Only
     bodies with a table-less indirect JMP acquire all instruction labels.
     CFG flag analysis includes these local successors too.

     The private fill reproducer was promoted into the instruction suite,
     with randomized zero-to-eight-byte fills and a second four-way store
     sequence selected by `LEA EAX,[base + ECX*4]; JMP EAX`. Both compare
     registers, flags and scratch bytes against Unicorn. The driver checks
     every local case and label, the fallback, and absence of the switch
     from a plain body. The focused red run was **3 failed, 1 passed**;
     implementation made the complete two focused suites **92 passed**.
     The broader suite initially found seven old structure-field jump tests
     expecting no switch at all. Their expectation now follows the approved
     two-level rule while retaining checks that no jump table was decoded.
     The final portable suite is **144 passed, 1 skipped**.

     Normal regeneration and the headless link **exit 0**
     (`build/task13-f6-regenerate.log`). Translation took **125.2 s**:
     **29,816/30,495 functions**, **38,126 entry points**, **4,028 candidates
     rejected as data**, 151 chunks. The existing linker section-alignment
     warning remains. The fresh boot passes the former fill-routine failure
     and reaches two `EnumCalendarInfoW` calls.

  7. The new log line is
     `call to unknown target 0082ce68 (ESP=0efffdf4, return=0fdfff00): returning 0`
     (`build/task13-run-06.log`). The verified caller listing
     `functions/0082cef8.asm` pushes `0x0082ce68` at `0x0082cfe2` and
     `0x0082d0c0` before the calendar-enumeration import. The callback has no
     exported function listing. Direct decoding of the pinned PE shows
     `55 8b ec` (`PUSH EBP; MOV EBP,ESP`) at that entry, a normal frame and
     SEH cleanup, and a final `RET 4` at `0x0082cef4`. It is executable code
     aligned to 8 bytes, not 16, following `MOV EAX,EAX` padding.

     `looks_like_function` and `looks_like_code_start` both require 16-byte
     alignment, and the callback exceeds the eight-instruction thunk limit.
     A direct probe returns false for all three predicates and for
     `plausible_immediate_target(0x0082ce68)`. Consequently the immediate scan
     skips the callback before recovery, and it has no generated entry.
     The synthetic ignored regression
     `build/task13_unaligned_callback_test.py` recovers an equivalent
     frame-based callback but **fails** its immediate-candidate assertion.
     There is **no fix commit for finding 7**. Accepting such callbacks needs
     an approved function-start/admission rule beyond the existing alignment
     and short-thunk signals; the task stops at that design boundary.

     Later in the same boot, after further locale/string calls, the host
     **exits 5** with
     `SIGBUS in guest thread 1: EIP=0080ae6b ESP=0efffbf4 EBP=0efffe10`.
     `functions/0080ae48.asm` places that guest address in Unicode-string
     assignment, just before a call to the string-release helper. The cause
     of this later fault, and whether it depends on the missing callback,
     remain unproven. Two LLDB attempts stalled at `run` before guest output:
     the ordinary batch launch and a PTY launch with ASLR left enabled.
     Both owned debugger/inferior groups were terminated; neither produced
     a backtrace (`task13-f7-lldb.log`, `task13-f7-lldb-noaslr.log`).

     As directed, `GetLogicalProcessorInformation` and
     `RtlCompareUnicodeString` remain documented zero-returning misses. The
     guest proceeds past both. No unhandled Delphi exception was logged, so
     no exception object was available to decode.

  The exact boot command was:
  `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
  RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
  build/recomp/pop_headless > build/task13-run-06.log 2>&1`.
  It retains the **600-frame / 180-second** cap and no thread override.
  `grep -c 'PeekMessageW\|MsgWaitForMultipleObjectsEx'
  build/task13-run-06.log` prints **0** (grep exits 1), and the frame
  directory contains **0 frame files**. Task 13's **600-frame, exit-0
  acceptance remains unmet**. The pinned executable SHA-256 was freshly
  verified unchanged.

  Validation from the game repository root (all logs under `build/`):

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py
    kit/tools/recomp/tests/test_translate_driver.py -k 'unrolled or tableless_jump'`:
    **3 failed, 1 passed, 88 deselected** before implementation
    (`task13-f6-red.log`).
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py
    kit/tools/recomp/tests/test_translate_driver.py`: **92 passed**
    (`task13-f6-focused.log`).
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py`:
    initial **7 failed, 137 passed, 1 skipped** (`task13-f6-portable.log`),
    then **144 passed, 1 skipped** (`task13-f6-portable-green.log`), retaining
    the prior corpus exclusions.
  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f6-regenerate.log 2>&1`: **exit 0**.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose
    > build/task13-f6-runtime.log 2>&1`: **844 checks, 0 failures, 1 skipped**,
    exit **0**, label `game`.
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose
    > build/task13-f6-seh.log 2>&1`: **113 checks, 0 failures**, exit **0**,
    label `nogame`. The existing helper uses the kit build/test wrappers;
    the current `tools/test.py` CLI has no `-R` option.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-f6-config.log 2>&1`: **7 passed**.
  - `.venv/bin/python -m pytest -q build/task13_unaligned_callback_test.py
    > build/task13-f7-unaligned-red.log 2>&1`: **1 failed**, the unresolved
    finding-7 admission regression.
  - Formatting, staged source-boundary, game-literal and whitespace checks
    passed before the kit commit. Generated code, assets, logs and diagnostic
    scripts remain private and ignored; nothing is pushed.


  **2026-09-14 continuation, after the orchestrator's per-game alignment decision:**

  7. Finding 7 is resolved by kit **5f76196**, `Translator: configure
     function-start alignment per game`. `game_config.load` supplies
     `[translate] function_alignment = 16` when omitted and rejects values
     that are not positive integers. Both function-start gates use the
     configured alignment; their remaining instruction/padding checks and
     the thunk rule remain. The stub documents 16. This game's setting is
     **4**, citing the listed-start histogram's mod-16 peaks at 0, 4, 8, 12.
     The promoted callback regression rejects alignment 16 and admits 4;
     config tests cover the default, override, validation and translator
     wiring, and this game's test requires 4.

     The tests were observed failing before implementation: **4 failed,
     1 passed** in the focused kit selection and **1 failed** in this
     game's new assertion. Afterwards, the complete kit config/translator
     config/driver suites passed **79 tests**, the game suite **5 tests**,
     and the portable translator suites **147 passed, 1 skipped**.
     A direct probe now admits `0x0082ce68`, and the regenerated table
     contains `fn_0082ce68`. Regeneration **exits 0**
     (`build/task13-f7-regenerate.log`): **280.8 s**, **32,931/35,373
     functions**, **43,331 entry points**, **14,115 candidates rejected as
     data**, 166 chunks, discovery converged in 19 rounds. The existing
     linker section-alignment warning remains.

     The new boot (`build/task13-run-07.log`) no longer logs the unknown
     calendar callback. It still **exits 5** at the same `SIGBUS`, guest
     EIP `0x0080ae6b`, before the message loop.

  8. The remaining warning is
     `SEH leave: no retired frame at ESP=0efffe1c`, followed later by the
     string-assignment SIGBUS. The requested noninteractive LLDB launch
     succeeds with `process launch --environment NAME=VALUE`.
     Its ordinary `-o` commands after launch do not run on a crash in this
     LLDB, so `-k 'bt 25' -k 'register read'` supplies the crash commands.
     The first attempt inspected the previous binary while regeneration
     ran (`build/task13-lldb.log`); the repeat inspected the rebuilt
     alignment-4 binary (`build/task13-f8-lldb.log`). The latter backtrace is
     `wr32 -> fn_0080ace4 -> fn_0080ae48 -> fn_0082c478 -> body_0082c4f0`.
     It faults while decrementing an invalid Unicode-string reference count,
     not in the preceding Move call. An additional frame-variable request
     failed because the selected inlined frame had no `c`; the backtrace
     and register dump before that error were produced successfully.

     Breakpoints immediately before and after the call from `0x0082c53c`
     to `0x0082cef8` isolate the earlier corruption
     (`build/task13-f8-stack.log`). Before the call, ESP is `0x0efffe4c`
     and EBP `0x0efffe9c`; on returning to the caller, ESP is incorrectly
     `0x0efffe1c`, EBP `0x0efffe44`, and EIP still `0x0082d177`, the
     initializer's internal continuation. Its generated RET at
     `0x0082d16f` returned the host frame before the guest epilogue restored
     the saved registers and stack.

     The verified `functions/0082cef8.asm` establishes a nested frame
     with `XOR EDX,EDX` at `0x0082cf3c`, followed by the three PUSHes and
     `MOV FS:[EDX],ESP` at `0x0082cf47`. The old recognizer only accepted
     absolute FS:[0] and FS:[EAX], so it found the outer frame but missed
     this nested frame and its finally ownership. Kit **ab57dd4**,
     `Translator: recognize SEH frames through zeroed registers`, recognizes
     the equivalent register spelling when the contiguous XOR and three
     PUSHes prove the base is zero. ESP is excluded because the PUSHes
     change it. The existing EAX/absolute spellings remain; other TEB
     operands do not acquire checkpoints. Emission uses the recognized
     establishing site for the new spelling.

     The new checkpoint and complete-cleanup driver regressions initially
     gave **2 failed, 1 passed**. An additional ESP negative case failed
     before the exclusion. Final focused suites give **72 passed**; the
     portable suite gives **151 passed, 1 skipped**. A direct listing probe
     now finds both establishing sites, `0x0082cf16 -> 0x0082d18d` and
     `0x0082cf47 -> 0x0082d170`. This reuses the already approved shared
     cleanup/RET ownership rule; it does not add a new return convention.


     Regeneration with that fix **exits 0**
     (`build/task13-f8-regenerate.log`): **345.0 s**, **33,098/35,540
     functions**, **44,613 entry points**, **14,110 candidates rejected as
     data**, 167 chunks, discovery converged in 21 rounds. The existing
     linker section-alignment warning remains. The next boot passes the
     calendar initializer without its SEH-leave warning or SIGBUS, reaches
     VCL font/monitor/OLE setup, and creates the `tapplication` window.
     It still **exits 6**, before the message loop, with the next finding.

  9. **Blocked on runtime-generated window-procedure thunks.** The first
     new unknown call is
     `call to unknown target 01141fe2 (ESP=0efffcb8, return=0fdfff00): returning 0`.
     LLDB stops there with the host chain
     `recomp_unknown_call -> recomp_call -> guest_call ->
     host_dispatch_to_wndproc -> send_message -> body_00a0f854`.
     This is delivery of message `0x80` to HWND `0x00020008` after the
     guest installed the heap address with `SetWindowLongW`.

     The captured guest allocation (`build/task13-f9-thunks.bin`) proves
     this is code generated at runtime, not another static callback
     admission problem. Its layout is:

     ```text
     01141fe2  e8 1d f0 ff ff       CALL 01141004
     01141fe7  18 03 a1 00         method = 00a10318 (data)
     01141feb  10 de 0c 01         object = 010cde10 (data)
     01141004  59                  POP ECX
     01141005  e9 aa 3c 74 ff       JMP 00884cb4
     ```

     The second installed thunk, `0x01141fef`, has the same form, with
     method `0x00a1129c` and the same object. Both methods and the common
     dispatcher `0x00884cb4` are already in the generated table.
     `functions/00884cdc.asm` verifies the producer: `VirtualAlloc` with
     protection `0x40`, a shared two-byte opcode template plus relative
     target, then 13-byte records with CALL opcode, relative displacement,
     method and object. The static dispatcher consumes ECX's record,
     invokes its method with the object in EAX, and returns with `RET 0x10`.
     The kit's existing dispatch covers translated entries and import
     trampolines; it has no executor/recognizer for these heap stubs.
     Supporting that form requires an explicit runtime dispatch design.
     No heap-code execution, hook address, or speculative workaround was
     added. **No fix commit for finding 9.**

     Later in the same run, `heap_alloc` refuses **547,618,816 bytes**
     against its **218,103,808-byte** arena. A breakpoint at `RaiseException`
     captures code `0x0eedfade`, seven information words, and exception
     object `0x010f9030`: class **EOutOfMemory**, actual message
     **`Put pf mdlory`** (already corrupted). This executable's VMT class
     name pointer is at **VMT - 0x38**, pointing to the short string at
     `0x0081fb5e`; the plan's guessed **-0x2c** points to code here.
     The live debugger dump confirms the class and message
     (`build/task13-f9-thunk-class.log`). The raise reaches `RtlUnwind`;
     it does not reach `recomp_seh_unhandled`, so no unhandled-exception
     printer was changed. The cause of the oversized allocation and
     corrupted text is not yet established and is not attributed to the
     thunk miss without evidence.

     Teardown also logs unknown static target `0x008de6f8`, then
     `no block entry for indirect jump to 0x0080a71a from 0x0080a4ad`,
     with ESP `0x0080ac33` outside the guest stack. These remain follow-up
     symptoms. A new optional lookup, `InitializeConditionVariable`,
     returns zero and is followed by critical-section initialization;
     `msctf.dll` is reported missing and execution proceeds. They remain
     unresolved because implementation stops at the heap-thunk design
     boundary. The previously approved zero misses for
     `GetLogicalProcessorInformation` and `RtlCompareUnicodeString` remain.

  This continuation's exact boot command was:

  ```sh
  RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1 RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames build/recomp/pop_headless > build/task13-run-08.log 2>&1
  ```

  **Acceptance is not met:** exit **6**, **0** log lines matching
  `PeekMessageW|MsgWaitForMultipleObjectsEx`, and **0** frame files. The
  frame-count and import-log switches are the actual host/runtime switches,
  not the plan's guessed names. No synchronous-thread override was used.
  The executable's SHA-256 was rechecked and remains
  `0c028b582632129a43ea67da6040ecc5d78a14e3bba06fcd2e4071b06a9ebd5b`.

  Validation commands and observed results for findings 7-8:

  - `.venv/bin/python -m pytest -q kit/tests/test_game_config.py
    kit/tools/recomp/tests/test_translate_config.py
    kit/tools/recomp/tests/test_translate_driver.py -k alignment
    > build/task13-f7-red.log 2>&1`: **4 failed, 1 passed, 74 deselected**,
    exit **1**, before implementation.
  - `.venv/bin/python -m pytest -q tests/test_game_config.py -k alignment
    > build/task13-f7-game-red.log 2>&1`: **1 failed, 4 deselected**,
    exit **1**, before the game setting.
  - `.venv/bin/python -m pytest -q kit/tests/test_game_config.py
    kit/tools/recomp/tests/test_translate_config.py
    kit/tools/recomp/tests/test_translate_driver.py
    > build/task13-f7-focused.log 2>&1`: **79 passed**, exit **0**.
  - `.venv/bin/python -m pytest -q tests
    > build/task13-f7-game-green.log 2>&1`: **5 passed**, exit **0**.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py
    > build/task13-f7-portable.log 2>&1`: **147 passed, 1 skipped**,
    exit **0**. The existing corpus-specific exclusions are retained.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_seh.py
    kit/tools/recomp/tests/test_translate_driver.py -k edx
    > build/task13-f8-red.log 2>&1`: **2 failed, 1 passed, 68 deselected**,
    exit **1**, before implementation.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_seh.py
    -k edx > build/task13-f8-stackbase-red.log 2>&1`:
    **1 failed, 2 passed, 7 deselected**, exit **1**, before excluding ESP.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_seh.py
    kit/tools/recomp/tests/test_translate_driver.py
    > build/task13-f8-focused-final.log 2>&1`: **72 passed**, exit **0**.
  - The same portable command with output
    `> build/task13-f8-portable.log 2>&1`: **151 passed, 1 skipped**,
    exit **0**.
  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f7-regenerate.log 2>&1` and the same command with
    `> build/task13-f8-regenerate.log 2>&1`: both **exit 0**.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose
    > build/task13-f8-runtime.log 2>&1`: **844 checks, 0 failures, 1 skipped**,
    **100% tests passed**, exit **0**, label `game`.
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose
    > build/task13-f8-seh.log 2>&1`: **113 checks, 0 failures**,
    **100% tests passed**, exit **0**, label `nogame`.
    The existing helper calls the kit build/test wrappers; the current
    `tools/test.py` CLI still has no `-R` option.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-final-config.log 2>&1`: **8 passed**, exit **0**.
  - `lldb --batch -s build/task13-f9-thunk.lldb -- build/recomp/pop_headless
    > build/task13-f9-thunk.log 2>&1` and the class-offset-corrected
    `build/task13-f9-thunk-class.lldb` script with output in
    `build/task13-f9-thunk-class.log`: both **exit 0**, capturing the
    thunk, callback stack and exception object. The scripts launch with
    `process launch --environment RECOMP_MAX_FRAMES=600
    --environment RECOMP_LOG=2 --environment RECOMP_FRAMES=build/task13-frames
    --environment RECOMP_PROFILE_DIR=build/task13-profile`.
  - Formatting, staged source-boundary, game-literal and whitespace checks
    passed before each kit commit. Generated output, logs, debugger scripts
    and guest-memory captures remain ignored; nothing is pushed.


  **2026-09-14 continuation, after the orchestrator's heap-thunk decision:**

  9. Finding 9 is implemented by kit **f735c5d**, `Runtime: interpret bounded
     compiler-generated guest thunks`. `runtime/thunks.cpp/.h` decodes at
     most 16 instructions in guest heap/stack arenas outside the image:
     relative CALL/JMP, short JMP, PUSH immediate, MOV immediate/register,
     indirect absolute-memory JMP, and register POP. POP is necessary for
     the decision's explicitly described `POP ECX` shared stub, although
     the opcode enumeration omitted it. A successful decode dispatches to a
     translated entry or trampoline, or returns at a recorded CALL
     continuation. Both unknown-call and unknown-jump paths use it, so
     user32's message callbacks use the same mechanism. No guest address
     is embedded in the runtime recognizer.

     Failed decoding restores trial CPU/stack changes, prints the stopping
     address and bytes, and retains the existing unknown-target behavior.
     Native tests cover a heap CALL whose pushed return names its record,
     the CALL/POP/JMP form, a direct JMP, a heap WNDPROC reached by
     `SetWindowLongW`/`SendMessageW`, a bounded loop with PUSH rollback, and
     stack code using the remaining MOV/short-JMP/indirect-JMP forms.
     The tests first gave **850 checks, 5 failures, 1 skipped**. Routing all
     test-only unknown image calls through the real fallback then exposed
     an unrelated TLS test seam expectation; the stand-in keeps its old
     no-translation behavior while sharing the new thunk execution path.
     The final runtime suite gives **851 checks, 0 failures, 1 skipped**.
     The generated-table regression first gave **1 failed, 61 passed**,
     then **62 passed**; it checks target classification and unknown-jump
     integration. An initial `-k call_return` selection matched no test
     (**62 deselected**, exit 5); the full driver suite supplied the actual
     failing run. The portable translator suite gives **151 passed,
     1 skipped**, and game-config/literal checks **8 passed**.

  10. **New design boundary: lossless x87 integer copies.** The resource
      table is correct: RT_STRING block **4096**, language **0**, ID
      **65532**, payload RVA **0x0049eba2**, contains `Out of memory`.
      A breakpoint immediately after `LoadStringW` copies it confirms
      identical text at image VA `0x00c9eba2` and output `0x0effdf64`, with
      capacity `0x1000` (`build/task13-f10-resource.log`). Those diagnostics
      used the preceding binary while the thunk regeneration ran. The
      resource walker and the wide shim did not corrupt these bytes.

      `functions/008107dc.asm` passes the result to Unicode assignment at
      `0x0080ae48`, which calls the copy routine at `0x00807088`. Its
      26-byte path uses four `FILD qword` loads and four `FISTP qword`
      stores, including overlapping final eight-byte chunks. Real x87
      preserves the full 64-bit integer. The kit converts it to `double`
      through `x87_int_value`/`fpush` and later `fto_i64`, losing bits above
      double's 53-bit integer precision. For the first four UTF-16 units,
      `0x002000740075004f` becomes `0x0020007400750050`: `Out ` becomes `Put `.

      The ignored reproducer `build/task13_fild_copy_test.py` uses the
      existing instruction suite's native/Unicorn harness. A single
      FILD/FISTP copy fails with `native 50 unicorn 4f` at the destination.
      A second case with the actual 26-byte load/store order produces
      exactly **`Put pf mdlory`**, while Unicorn preserves **`Out of memory`**.
      Both fail (**2 failed**, `build/task13-f10-copy-red.log`). No passing
      result is claimed and the failing diagnostic is not committed.
      Fixing this needs a decision about the existing `double st[8]` x87
      representation (for example, retaining exact integer payloads across
      integer-copy operations versus changing the floating representation).
      No representation change or copy-routine hook was made.

  11. The oversized allocation is traced separately in
      `build/task13-f10-allocation.log`. At entry to `0x0080ac14`, EAX is
      **0x1051ff08 = 273,809,160** UTF-16 units and EDX is **0x0088c840**.
      The caller `0x0080b0a0` read the alleged string length from `[EDX-4]`.
      This is a **code address**, the continuation after `CALL [ECX+0x10]`
      at `0x0088c83d`, not a string. Bytes at `0x0088c83c..0x0088c83f`
      are `08 ff 51 10`, exactly the erroneous length. The allocator asks
      for `2 * length + 14 = 547,618,334` bytes; its rounded VirtualAlloc
      request is **547,618,816** bytes. No system-memory, disk-space or
      resource-size shim directly supplied that number: it came from
      instruction bytes treated as string metadata. The preceding
      MultiByteToWideChar call is not assigned blame without evidence.
      The host chain is `fn_0080ac14 -> fn_0080b0a0 -> fn_0088c94c ->
      fn_0088cecc -> body_00861ed8`. The earlier pointer/stack corruption
      that supplied the code address remains unresolved at the x87
      design boundary.

  12. The teardown addresses were checked before considering more SEH
      seeds. Neither exact address is in a listed function's exported
      instruction rows. `0x0080a4ad` is in the recovered RTL helper
      starting at **0x0080a480**, between listed `0x0080a3bc` and
      `0x0080a4b0`; its bytes decode to `JMP EDX` after restoring ESP,
      FS:[0], EBP and calling `0x00809fec`. The target **0x0080a71a** lies
      in omitted handler code following listed `0x0080a6b8`, between that
      function's normal jump and its epilogue. Decoding the actual bytes:

      ```text
      0080a70a  JMP 0080a028
      0080a70f  CALL 0080a650
      0080a714  CALL 0080a42c
      0080a719  CALL 0080a480
      0080a71e  POP EDI
      ```

      Thus **0x0080a71a is inside the CALL's displacement**, not a valid
      new landing instruction. The generated table already has the valid
      epilogue entry **0x0080a71e**. No speculative landing seed was added;
      the bad target remains a corrupted-control-flow symptom.


  Validation and diagnostic commands for this continuation (all output
  remains under ignored `build/`):

  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose
    > build/task13-f9-red.log 2>&1`: **850 checks, 5 failures, 1 skipped**,
    exit **1**, before implementation.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    > build/task13-f9-driver-red.log 2>&1`: **1 failed, 61 passed**, exit **1**.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose
    > build/task13-f9-green.log 2>&1`: final **851 checks, 0 failures,
    1 skipped**, exit **0**, label `game`.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    > build/task13-f9-driver-green.log 2>&1`: **62 passed**, exit **0**.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py
    > build/task13-f9-portable.log 2>&1`: **151 passed, 1 skipped**,
    exit **0**, retaining the existing corpus exclusions.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-f9-config.log 2>&1`: **8 passed**, exit **0**.
  - `lldb --batch -s build/task13-f10-resource.lldb -- build/recomp/pop_headless
    > build/task13-f10-resource.log 2>&1`: **exit 0**, source and shim output
    both `Out of memory`.
  - `lldb --batch -s build/task13-f10-allocation.lldb -- build/recomp/pop_headless
    > build/task13-f10-allocation.log 2>&1`: **exit 0**, captures the invalid
    string pointer and length before allocation. Both debugger scripts use
    `process launch --environment RECOMP_MAX_FRAMES=600
    --environment RECOMP_LOG=2 --environment RECOMP_FRAMES=build/task13-frames
    --environment RECOMP_PROFILE_DIR=build/task13-profile`.
  - `.venv/bin/python -m pytest -q build/task13_fild_copy_test.py
    > build/task13-f10-fild-red.log 2>&1`: initial single-copy reproducer
    **1 failed**, exit **1**.
  - `.venv/bin/python -m pytest -q -s build/task13_fild_copy_test.py
    > build/task13-f10-copy-red.log 2>&1`: expanded reproducer **2 failed**,
    exit **1**; prints `native copied text 'Put pf mdlory'`.
  - `.venv/bin/python kit/tools/format.py --write`, staged
    `kit/tools/check_repo.py`, `kit/tools/check_game_literals.py`, and
    whitespace checks passed before the kit commit. No private diagnostic
    scripts, guest bytes, generated code or logs are committed.


  The final thunk-enabled regeneration and run are complete:

  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f9-regenerate.log 2>&1`: **exit 0**. Translation took
    **338.3 s**, with **33,098/35,540 functions**, **44,613 entry points**,
    **14,110 candidates rejected as data**, 167 chunks and 21 discovery
    rounds. The existing linker section-alignment warning remains.
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose
    > build/task13-f9-seh.log 2>&1`: **113 checks, 0 failures**, exit **0**,
    label `nogame`.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-09.log 2>&1`: **exit 6**.
    Both heap WNDPROC misses are gone. The message at `0x00a0f9dd` now
    reaches the guest window method, which calls `DefWindowProcW` with
    guest return `0x00a102ba`. The same oversized allocation, later unknown
    static target `0x008de6f8`, and invalid jump `0x0080a71a` still occur.
  - `lldb --batch -s build/task13-f9-final-probes.lldb -- build/recomp/pop_headless
    > build/task13-f9-final-probes.log 2>&1`: **exit 0**, verifying the
    rebuilt binary. `fn_00884cb4` is reached through `recomp_run_thunk` with
    **ECX=0x01141fe7**, exactly the record address after the heap CALL, and
    **ESP=0x0efffcb8**, the original callback stack position. The same run
    reconfirms correct LoadStringW output and the invalid string pointer
    **EDX=0x0088c840**, length **EAX=0x1051ff08**.

  **Acceptance remains unmet:** zero `PeekMessageW|MsgWaitForMultipleObjectsEx`
  log lines, zero frame files, exit 6. No synchronous-thread override or
  hash bypass was used. The executable's SHA-256 was rechecked and remains
  `0c028b582632129a43ea67da6040ecc5d78a14e3bba06fcd2e4071b06a9ebd5b`.
  Implementation stops at finding 10's x87 representation decision; the
  invalid string pointer and teardown target are separately documented,
  without assigning an unproven common cause. The optional
  InitializeConditionVariable lookup and the previously approved zero
  misses remain unchanged. Nothing is pushed.

- **2026-09-14 — Task 13 continuation: exact x87 integer copies (finding 10).**
  The approved integer side channel is implemented in kit **4a47e88**
  (`x87: preserve exact integers through FILD and FIST stores`). This
  extends the preceding findings; their earlier run results remain above.

  10. The reproduced `native copied text 'Put pf mdlory'` corruption is
      fixed by retaining signed 64-bit FILD payloads alongside each x87
      double. FIST/FISTP stores use the exact integer when representable;
      narrower overflow keeps the existing integer-indefinite behavior.
      FLD ST, FXCH and FST/FSTP ST preserve the payload and validity flag;
      arithmetic, ordinary loads and restored floating values invalidate
      it. Both Python X86 mirrors were updated. Whole-context copies in
      SEH and heap-thunk dispatch already carry the new fields, and
      `stub_recomp_call.cpp` has no field-by-field context copy to change.
      The existing mod ABI exposes doubles: unchanged hook values retain
      the internal exact payload, while changed values clear it. Replay
      creates a zero-initialized X86 from that double-only ABI, so it has
      no stale exact flags. Extending the external capture ABI is outside
      this integer-copy fix. The listed Move routine at `0x00807088`
      uses qword integer copies; no FLD/FSTP tbyte copy was found there,
      and 80-bit memory conversion behavior was not expanded.

  Validation before regeneration:

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py
    -k FILD > build/task13-f10-red.log 2>&1`: **10 failed, 9 passed,
    33 deselected**, exit **1**, before implementation. This includes
    the promoted single-copy and 26-byte resource-copy reproducers.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py
    > build/task13-f10-green.log 2>&1`: **52 passed**, exit **0**.
    The new cases cover the requested fixed and random qwords, register
    moves, sign extension, narrowing and stale metadata after arithmetic
    or stack-slot reuse, all compared with Unicorn.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py
    > build/task13-f10-portable.log 2>&1`: **170 passed, 1 skipped**,
    exit **0**, retaining the earlier corpus exclusions.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose
    > build/task13-f10-native.log 2>&1`: **851 checks, 0 failures,
    1 skipped**, exit **0**, label `game`.
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose
    > build/task13-f10-seh.log 2>&1`: **113 checks, 0 failures**,
    exit **0**, label `nogame`.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-f10-config.log 2>&1`: **8 passed**, exit **0**.
  - `.venv/bin/python kit/tools/format.py --write`, staged
    `kit/tools/check_repo.py`, `kit/tools/check_game_literals.py`, and
    whitespace checks passed before the kit commit. The executable hash
    was rechecked and still matches the pinned SHA-256.

  Regeneration and the post-fix run:

  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f10-regenerate.log 2>&1`: **exit 0**. Translation took
    **330.6 s**, with **33,098/35,540 functions**, **44,613 entry points**,
    167 chunks and the same recovery counts as the preceding run. The
    existing linker section-alignment warning remains.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-10.log 2>&1`: **exit 6**.
    The promoted oracle now copies `Out of memory` exactly, but the
    headless run still rejects the oversized allocation. No claim is made
    that the exception object's message was re-read successfully in this
    run: the debugger probes described below never reached guest code.

  11. **Re-evaluated after exact copies: still unresolved.** The new log
      repeats `heap_alloc: refusing a 547618816 byte request, the arena is
      218103808 bytes`, followed by RaiseException at guest return
      `0x0080ac27` and RtlUnwind. Thus fixing FILD/FISTP precision did not
      remove the oversized-allocation symptom. The earlier live diagnosis
      of code address `0x0088c840` as a string pointer remains the starting
      point, rather than a newly verified register capture. A prepared
      probe compares ESP around the indirect call at `0x0088c83d`, then
      stops at the allocation and decodes the exception object; it could
      not complete because of the debugger launch blocker below.

  12. **Re-evaluated after exact copies: still unresolved.** The new log
      repeats `call to unknown target 008de6f8 (ESP=0efffe58,
      return=0080a690): returning 0`, then `no block entry for indirect
      jump to 0x0080a71a from 0x0080a4ad`. The abort reports
      **EIP=0080a4ad, ESP=0080ac33, EBP=89000000**. The target is still
      inside the CALL displacement documented above. No invalid landing
      seed or new control-flow rule was introduced.

  Diagnostic limitation in this continuation:

  - `lldb --batch -s build/task13-f10-recheck.lldb -- build/recomp/pop_headless
    > build/task13-f10-recheck.log 2>&1` stalled at `process launch`,
    before any guest output. The source breakpoint resolved successfully.
  - Retrying that command with a PTY (output in
    `build/task13-f10-recheck-pty.log`), then with
    `process launch --disable-aslr false --no-stdio` in
    `build/task13-f10-recheck-nostdio.lldb` (output in
    `build/task13-f10-recheck-nostdio.log`), stalled at the same point.
    A temporary copy of the native executable under `/tmp`, using the
    latter script, also stalled (`build/task13-f10-recheck-copy.log`).
  - One-second `sample` captures of the owned debugger, debugserver and
    inferior show LLDB waiting for a process stop, debugserver waiting
    for process events, and the inferior blocked in
    `dyld4::Loader::getOnDiskBinarySliceOffset -> mapFileReadOnly -> __open`
    before `main`. These are diagnostic observations, not a proven cause
    of the debugger stall. All four attempts were explicitly terminated;
    none is reported as a passing run. Their scripts, logs and samples
    remain ignored under `build/`; the temporary executable copy was
    removed, and no owned debugger or headless process remains.

  **Acceptance remains unmet:** **0** `PeekMessageW|MsgWaitForMultipleObjectsEx`
  lines, **0** frame files, exit **6**, using the actual `RECOMP_MAX_FRAMES`
  cap and `RECOMP_LOG=2` import tracing. GetLogicalProcessorInformation and
  RtlCompareUnicodeString remain the approved zero misses;
  InitializeConditionVariable also remains an observed miss without a
  proven causal link to this abort. No synchronous-thread override, hash
  bypass, new shim assumption or unrelated host/debugger repair was made.
  Work stops with the tested x87 fix preserved and findings 11/12 awaiting
  a working live probe or another approved diagnostic route.

- **2026-09-14 — Task 13 continuation: decoded CALL returns fixed;
  heap diagnostics identify a register-pushed RET adapter.** Continued from
  kit `4a47e88` and game commit `0dde4a1`, using the orchestrator's decisions
  on findings 11 and 12. Earlier findings above remain the historical record.
  No debugger was used in this continuation.

  12. **Fixed emission, not decoding — kit `c550d31`
      (`Translator: push decoded CALL continuations at block boundaries`).**
      The old generated `fn_0080a70f` contained all three correctly aligned
      CALL instructions, but its last CALL at `0x0080a719` pushed
      `0x0080a71a`. A recovered body's estimated end is the last instruction
      address plus one; emission used that estimate for its final CALL.
      `Function.measure` already retained the actual decoded next address
      in `fallthrough`, `0x0080a71e`. Emission now uses that decoded address
      when available, retaining the listing fallback otherwise. The new
      driver regression establishes an SEH frame, jumps with EB over an E9
      handler stub and three omitted CALLs, and includes a DoneExcept-style
      `POP EDX; JMP EDX` helper. It failed on the final pushed address before
      the change and passes afterwards. Regenerated C now pushes
      `0x0080a71e`; the subsequent runs no longer report the indirect jump
      into the CALL displacement at `0x0080a71a`.

  11. **Permanent refusal diagnostic — kit `1686c72`
      (`Runtime: report guest context when heap requests exceed the arena`).**
      `heap_alloc` now logs the active guest context's registers, up to 12
      EBP-linked returns, and up to 24 CALL-preceded stack candidates for
      frameless RTL helpers. The isolated native child test observes the
      actual refusal log and checks the size, register values and frame
      return. Its two new diagnostic assertions failed before implementation.
      The rebuilt run still reports:

      ```text
      heap_alloc: refusing a 547618816 byte request, the arena is 218103808 bytes
      heap_alloc: EIP=00805310 EAX=20a3fe1e ECX=8bd68b00 EDX=04147fc4 EBX=010f9424 ESP=0efffed8 EBP=20a3fe1e ESI=20a40000 EDI=7ffffffd
      ```

      EBP is currently an allocation size, so the safe frame walker emits
      no EBP frames here. The stack candidates include, in order,
      `0080578d 00806ece 0080ac27 0080b0b4 0088c840 0088c96e 0088c840
      0088cf2a 0088ceef 0088c840 0088ceef 00861f3e 00862871 00862dd3
      00bfe47d 0080a6fc 0080a768 00811f0f 00bfea50`. These are candidates,
      including corrupt data slots, rather than a claimed exact call stack.
      They confirm the dictionary insertion and string-allocation path.

  13. **New design boundary explaining finding 11: RET as a vtable-adapter
      jump; no fix commit.** An ignored guest-memory snapshot taken at the
      refusal identifies the live dictionary at `0x01116070`, its comparer
      interface at `[dictionary+0xc] = 0x010e5a60`, and the interface vtable
      at `0x008fe6d0`. The indirect CALL at `0x0088c83d` therefore selects
      `[vtable+0x10] = 0x008fe67a`. The pinned PE and generated C agree on
      this six-instruction adapter:

      ```text
      008fe67a ADD EAX,-8
      008fe67d PUSH EAX
      008fe67e MOV EAX,[EAX]
      008fe680 MOV EAX,[EAX+8]
      008fe683 XCHG [ESP],EAX
      008fe686 RET
      ```

      The adjusted object is `0x010e5a58`; its vtable is `0x008fe470`, whose
      method at offset 8 is `0x008ffffc`. On x86, the adapter's RET pops that
      method address and executes the method, leaving the original caller's
      return address for the method's eventual RET. Generated C instead
      pops the method into EIP and returns directly to `fn_0088c830`, without
      calling the method. This function has no pushed immediate continuation,
      so the approved finding-4 rule deliberately emits a plain RET.

      Consequently `fn_0088c830` resumes with ESP four bytes below the correct
      value. Its POP ESI consumes `0x0088c840`, its POP EBX consumes the saved
      string pointer, and its RET consumes the saved dictionary pointer.
      The snapshot matches those effects: the original UTF-16 string at
      `0x010f9424` is `TMenuItem`, but the later insertion's `[EBP+0xc]` at
      `0x0effff30` contains `0x0088c840`. `0x0080b0a0` reads the preceding
      instruction bytes as length `0x1051ff08`, causing the oversized
      request. This identifies a translator stack/control-flow error, not
      a shim argument count, resource length, memory-status or disk-size
      answer.

      Private regression `build/task13_vtable_ret_test.py` uses the same
      adapter with a synthetic object/vtable and a method that sets EAX then
      returns. The existing instruction harness compares it with Unicorn:
      **1 failed**, with `EAX: native 0e100100 unicorn 12345678` and
      `ESP: native 0effff00 unicorn 0effff04`. No failing regression or
      special-case game address was committed. This requires an explicit
      extension of RET handling to register-pushed/XCHG targets; the approved
      immediate-continuation rule and heap-code interpreter do not cover it.
      Work stops here rather than changing all RET dispatch semantics.

  Commands and observed checks in this continuation:

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k except_calls > build/task13-f12-red.log 2>&1`: **1 failed,
    62 deselected**, exit **1** before the emission fix.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    kit/tools/recomp/tests/test_translate_insns.py > build/task13-f12-green.log
    2>&1`: **115 passed**, exit **0** after the fix.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose
    > build/task13-f11-diag-red.log 2>&1`: **857 checks, 2 failures,
    1 skipped**, exit **1** before implementing the refusal diagnostic.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose
    > build/task13-f11-diag-green.log 2>&1`: **857 checks, 0 failures,
    1 skipped**, exit **0**, label `game`.
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose
    > build/task13-f12-seh.log 2>&1`: **113 checks, 0 failures**, exit **0**,
    label `nogame`. The existing ignored wrapper drives `tools/test.py`;
    this checkout's root CLI does not accept the plan's `-R` option.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py
    > build/task13-f12-portable.log 2>&1`: **171 passed, 1 skipped**, exit **0**,
    retaining the earlier corpus exclusions.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-f12-config.log 2>&1`: **8 passed**, exit **0**.
  - `.venv/bin/python -m pytest -q build/task13_vtable_ret_test.py
    > build/task13-f13-red.log 2>&1`: **1 failed**, exit **1**, the intentionally
    unresolved design-boundary reproducer described above.
  - `.venv/bin/python kit/tools/format.py --write`, staged
    `kit/tools/check_repo.py`, `kit/tools/check_game_literals.py`, and
    whitespace checks passed before each kit commit. The original executable
    hash was checked again and remains the pinned SHA-256.

  Build and run evidence:

  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f12-regenerate.log 2>&1`: **exit 0**. Translation took
    **329.5 s**, still **33,098/35,540 functions**, **44,613 entry points**,
    and 167 chunks. The existing linker section-alignment warning remains.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-11.log 2>&1`: **exit 6**.
  - For the private snapshot only, temporary generic code read
    `recomp_env("HEAP_REFUSAL_DUMP")` and wrote guest memory to its path on
    refusal. `.venv/bin/python tools/build.py --target headless --jobs 8
    > build/task13-f11-probe-build.log 2>&1` passed; the same run switches plus
    `RECOMP_HEAP_REFUSAL_DUMP=build/task13-heap-refusal.bin` wrote
    `build/task13-f11-probe.log` and exited **6**. The diagnostic source was
    then removed, and `git -C kit diff --exit-code` passed. The snapshot and
    reproducer remain private, ignored inputs under `build/`; no snapshot
    switch was added to the committed runtime.
  - `.venv/bin/python tools/build.py --target headless --jobs 8
    > build/task13-f11-restored-build.log 2>&1`: **exit 0**, rebuilding the
    committed source after removing the temporary probe.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-12.log 2>&1`: **exit 6**.
    Final error: `SEH: registration outside guest stack
    (registration=0080ac63 target=00000000 FS=0fe00000 ESP=0080ac63)`;
    the abort reports **EIP=fe0244c7, ESP=0080ac63, EBP=8338700e**.
    The previously observed unknown teardown target `008de6f8` remains;
    neither teardown symptom is treated as an independent design problem
    before correcting the proven earlier stack corruption.

  **Acceptance remains unmet:** the final run has **0**
  `PeekMessageW|MsgWaitForMultipleObjectsEx` lines and **0** frame files,
  exits **6**, and never demonstrates the VCL message loop. The actual
  `RECOMP_MAX_FRAMES=600` cap counts presented frames. The two approved
  optional GetProcAddress misses remain unchanged, as does the unproven
  InitializeConditionVariable miss. No synchronous-thread override, hash
  bypass, unrelated shim, or debugger repair was introduced. The tested
  commits are retained; the next required decision is how to recognize and
  dispatch the register-pushed vtable-adapter RET exactly once.

- **2026-09-14 — Task 13 continuation: RET classifies vtable-method targets.**
  Continued from kit `1686c72` and game commit `779d0e9`, following the
  orchestrator's finding-13 decision. Earlier findings are retained above.

  13. **Fixed — kit `c6adb32` (`Translator: dispatch RET targets that name
      methods`).** Every emitted RET still pops its target and applies its
      immediate stack adjustment once. The shared `recomp_return` helper
      first checks the generated call-return table; a match returns to the
      pending host caller even if it is also an alternate entry. Otherwise,
      a translated entry is dispatched through `recomp_call` as a tail call,
      so its RET consumes the original caller's return address. Unknown
      return targets retain the previous EIP/host-return behavior. Functions
      with pushed interior continuations still check their local switch
      before invoking this classification in its default arm. Plain functions
      keep compact one-line RET emission, without an interior switch; their
      classification lives in the common helper.

      The private adapter reproducer was promoted into the instruction suite
      with a small method in the case's own code range. The harness now exposes
      explicit alternate entries and the two table predicates, while using the
      runtime helper itself for RET classification. Before implementation,
      native execution skipped the method and left ESP four bytes below the
      Unicorn result. The method and caller now execute exactly once, and all
      compared registers, flags and memory agree. The driver regression checks
      compact plain RET emission and classification ordering; existing local
      continuation-switch regressions remain covered. The native SEH landing
      fixture also uses the new helper with a call-return sentinel that is
      deliberately an entry, exercising the precedence rule during unwinding.

  Checks for this fix:

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py
    kit/tools/recomp/tests/test_translate_driver.py
    > build/task13-f13-promoted-red.log 2>&1`: **3 failed, 114 passed**, exit
    **1**, before implementation (adapter behavior, switch default and plain
    RET classification assertions).
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py
    kit/tools/recomp/tests/test_translate_driver.py
    > build/task13-f13-green.log 2>&1`: **117 passed**, exit **0**.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py
    > build/task13-f13-portable.log 2>&1`: **173 passed, 1 skipped**, exit
    **0**, retaining the previous corpus exclusions. The isolated dispatcher
    probe in `test_translate.py` gained the new predicate stubs so it can link
    generated RET code; its corpus suite was not run.
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose
    > build/task13-f13-seh.log 2>&1`: **113 checks, 0 failures**, exit **0**,
    label `nogame`.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose
    > build/task13-f13-native.log 2>&1`: **857 checks, 0 failures, 1 skipped**,
    exit **0**, label `game`. The existing ignored wrapper drives kit
    `tools/test.py`; the root CLI still lacks the plan's `-R` option.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-f13-config.log 2>&1`: **8 passed**, exit **0**.
  - `.venv/bin/python kit/tools/format.py --write`, staged
    `kit/tools/check_repo.py`, `kit/tools/check_game_literals.py` and
    whitespace checks passed before the kit commit.

  Run and next finding:

  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f13-regenerate.log 2>&1`: **exit 0**, translation **327.6 s**,
    **44,613 entries** and **167 chunks**. The existing linker section-alignment
    warning remains. The generated header includes the committed RET helper.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-13.log 2>&1`: **exit 6**.
    The oversized allocation from finding 11 and the code-address ESP from
    finding 13 disappear. Startup progresses through launch-settings form
    loading; this is not evidence of the message loop running.

  14. **Blocked — normal flow joins an except-owned suffix before an outer
      finally cleanup; no fix commit.** The final visible error is
      `call to unknown target 00000001 (ESP=0efffe90, return=00808e63):
      returning 0`, followed by `SEH: registration outside guest stack
      (registration=00000000 target=010a8720 FS=0fe00000 ESP=0efffe80)`.
      A temporary call trace identifies the earlier divergence inside the
      reader at `00879224`, before the invalid virtual call or unwind:

      - The pinned PE decodes `00879507 JMP 00879530`, skipping the
        `00879509` handler stub and its except block at `0087950e`.
        The join at `00879530` pops the next registration, restores FS:[0],
        and executes `00879538 PUSH 0087954f`.
      - Normal flow then falls into cleanup `0087953d`, which calls a virtual
        method and RETs at `00879547`. Its exception entry is
        `00879548 JMP HandleFinally; 0087954d JMP 0087953d`. The normal
        continuation `0087954f` unlinks another frame and pushes `00879571`
        before another cleanup at `0087955c`; `00879571` is the actual
        epilogue, restoring ESP from EBP, popping EBP and returning.
      - Generated `body_00879224` instead ends at `00879507` with
        `CALL_FN(00879530); return;`. That entry belongs to
        `body_0087950e`, which stops after pushing `0087954f` and tail-calls
        cleanup `0087953d`. The cleanup belongs to `body_0087954d` and uses
        plain RET classification; its pushed continuation is outside its
        body. `0087954f` is absent from the generated entry table. Thus the
        RET returns through the pending C frames without executing either
        outer continuation or the original epilogue.
      - The trace confirms `00879224` enters with ESP=`0efffec8` and exits
        with ESP=`0efffe70`, EBP=`0efffec4`, EIP=`0087954f`. Its proper return
        position is `0efffecc`: **92 bytes higher**. Its caller `00873888`
        then runs cleanup using the reader's stale EBP and ultimately pops
        a UTF-16 form-name pointer as EBP/EIP. The invalid destructor target
        and SEH registration are downstream symptoms.

      This differs from the directly adjacent cleanup edges covered by
      finding 4: a normal branch first enters a join already owned by an
      except landing, and the finally cleanup itself belongs to a third
      body. The required decision is how to reunite these normal-flow
      fragments with the establishing function while preserving the
      exception-only entry and the two outer cleanup alternate entries.
      No speculative ownership expansion or unrelated shim change was made.

      `.venv/bin/python -m pytest -q build/task13_nested_cleanup_owner_test.py
      > build/task13-f14-reproducer.log 2>&1`: **1 failed**, exit **1**.
      This private synthetic driver fixture has an outer finally, an inner
      except, a direct jump to their shared join, and a pushed cleanup
      continuation. Translation succeeds, but the assertion that the
      cleanup belongs to the establishing body fails. Its source and
      generated evidence remain ignored under `build/` for the next decision.

  Diagnostic and final verification details:

  - Temporary runtime probes saved guest memory at the unknown call and
    traced calls via the existing profile hooks. The effective trace command
    was `RECOMP_PROFILE=1 RECOMP_CALL_TRACE_START=00879224
    RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-f14-call-trace.log 2>&1`:
    **exit 6**. The earlier snapshot run added
    `RECOMP_UNKNOWN_DUMP=build/task13-unknown.bin` to the ordinary run
    switches and wrote `build/task13-f14-probe.log`, also **exit 6**.
    A shim register probe reported no nonvolatile-register changes.
    An attempted inline RET probe was ineffective: the ordinary rebuild
    retains the generated copy of `x86.h`. Absence of that probe's output
    is not evidence about RET targets; the working profile trace above
    establishes the divergence instead. No generated file was edited.
  - All temporary probes were removed; `git -C kit diff --exit-code` passed.
    `.venv/bin/python tools/build.py --target headless --jobs 8
    > build/task13-f14-restored-build.log 2>&1`: **exit 0**.
  - Final committed-source run:
    `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-14.log 2>&1`: **exit 6**,
    with the same unknown target and invalid registration. Abort context:
    **EIP=00805174, ESP=0efffe80, EBP=01100c60**. There are **0**
    `PeekMessageW|MsgWaitForMultipleObjectsEx` lines, **0** frame files,
    **0** oversized heap refusals, and no remaining probe output.
  - The approved optional GetLogicalProcessorInformation and
    RtlCompareUnicodeString misses remain unchanged. InitializeConditionVariable,
    DirectXFileCreate and missing msctf/d3dxof/uxtheme modules were observed;
    the run proceeds past them, and none is established as the cause of this
    stack divergence. The pinned executable SHA-256 was rechecked and matches
    `0c028b582632129a43ea67da6040ecc5d78a14e3bba06fcd2e4071b06a9ebd5b`.

  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-final-config.log 2>&1`: **8 passed**, exit **0**.
    The generated `x86.h` matches the committed runtime header byte for byte.

  **Acceptance remains unmet.** The configured 600-presented-frame limit is
  never reached; Task 13 stops at finding 14's ownership decision.


- **2026-09-14 — Task 13 continuation: span-bounded pushed continuations.**
  Continued from kit `c6adb32` and game commit `f26d545`, applying the
  orchestrator's finding-14 decision. Earlier observations remain above.

  14. **Fixed in the translator — kit `4f6e528` (`Translator: recover pushed
      continuations within listed function spans`).** A listed function's
      span ends at the next original TSV function start, capped at its
      executable section end. Newly recovered entries do not shorten it.
      PUSH immediates within that span seed clean omitted code into the
      establishing body. Discovery repeats as recovered blocks expose more
      PUSHes, so the existing RET switch includes the complete continuation
      chain. Out-of-span targets retain the existing dispatch behavior.

      Recovery reuses the existing instruction sweep and direct-branch
      traversal, with explicit span bounds. It rejects undecodable code,
      padding reached before a block terminates, and instructions crossing the
      span boundary; emitter validation and the existing invalid-target checks
      still apply. Handler stubs retain their separate entries. Their in-span
      landing blocks are attached as alternate entries in the establishing
      body, allowing a normal path to join an except suffix before reaching
      an outer finally cleanup. Overlapping recovered fragments retire into
      that owner without promoting speculative entry provenance.

      The previous private nested-cleanup reproducer is now in
      `test_translate_driver.py`. A two-link omitted PUSH/RET chain checks
      labels and switch cases in the owning body; companion cases check the
      next listed boundary, rejection of UD2, padding and a truncated
      instruction. Existing tests that expected standalone continuation
      functions now inspect their owning body instead, preserving their
      decoded-CALL-return assertions. The initial rejection fixture used
      PUSH ES, which the emitter supports; it was corrected to UD2.

  Checks:

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k 'nested_except_join or span_recovers'
    > build/task13-f14-promoted-red.log 2>&1`: **4 failed, 64 deselected**,
    exit **1**, before implementation. The nested ownership and both valid
    continuation-chain cases fail before the change.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    kit/tools/recomp/tests/test_translate_insns.py
    > build/task13-f14-green.log 2>&1`: **123 passed**, exit **0**, after
    implementation and the additional boundary checks.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py
    > build/task13-f14-portable.log 2>&1`: **179 passed, 1 skipped**, exit
    **0**. The existing corpus exclusions remain. The final adjustment to
    derive boundaries from every original TSV row was subsequently covered
    by the 123-test instruction/driver run above.
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose
    > build/task13-f14-seh.log 2>&1`: **113 checks, 0 failures**, exit **0**,
    label `nogame`. The existing wrapper is retained because the root native
    CLI lacks `-R`.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-f14-config.log 2>&1`: **8 passed**, exit **0**.
  - `.venv/bin/python kit/tools/format.py --write
    > build/task13-f14-format.log 2>&1`: **exit 0**, 268 handwritten files
    formatted. `kit/tools/check_repo.py`, `kit/tools/check_game_literals.py`
    and the staged whitespace check passed before the kit commit.

  Regeneration and the next finding:

  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f14-regenerate.log 2>&1`: **exit 0**. Translation took
    **356.0 seconds**, emitting **30,090/32,532 functions, 44,113 entries,
    152 chunks**, after 20 discovery rounds. The table audit reports 277
    decoded tables, 3,709 entries, zero dispatches to nowhere and zero
    undecoded tables. The existing linker section-alignment warning remains.
    Generated `body_00879224` now contains the omitted continuation chain,
    including RET cases for `0087954f` and `00879571`, and the real epilogue
    at `L_00879571`. The earlier unknown target `00000001` disappears.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-15.log 2>&1`: **exit 6**.
    Startup advances to window-class registration, then reaches finding 15.

  15. **Blocked on a new candidate-admission rule; no fix commit.** The log
      reports `call to unknown target 00a09a30 (ESP=0efffbc4,
      return=00961bcc): returning 0`. The caller at `00961bc6` performs
      `CALL dword ptr [ECX+0xe4]`. The target is real, unlisted code with
      `PUSH EBP; MOV EBP,ESP; ADD ESP,-0x274; PUSH EBX; PUSH ESI; PUSH EDI`,
      followed by an SEH frame. Seven relocated vtable slots point to it:
      `0098a020`, `0098b64c`, `009fcdd0`, `009fe38c`, `009ff92c`,
      `00bd2028`, and `00beb598`.

      The preceding UTF-16 `MDICLIENT` string at `00a09a1c` is referenced by
      `MOV EDX,00a09a1c` at `00a098e3` in listed function `00a0981c`.
      Immediate scanning admits the string as speculative code. Its sweep
      misdecodes bytes at `00a09a2f` as a three-byte ADD spanning the real
      prologue, then emits IN at `00a09a32`. The real method start becomes
      an interior byte of that speculative instruction, so `code_pointers`
      excludes it before evaluating its entry evidence. The generated
      `body_00a09a1c` remains speculative, as finding 5 requires; its
      instructions and targets pass the existing pruning checks. It does
      not begin with the rejected `00 00` encoding. The next listed start
      is `00a09da0`; this is separate from finding 14's cleanup ownership.

      A synthetic fixture with the same UTF-16 prefix, immediate reference,
      relocated method pointer and alignment 4 reproduces the missing entry:
      `.venv/bin/python -m pytest -q build/task13_wide_prefix_entry_test.py
      > build/task13-f15-reproducer.log 2>&1`: **1 failed**, exit **1**.
      Translation succeeds but omits the real method's dispatcher entry.
      Source and generated evidence remain ignored under `build/`.
      Choosing whether to reject wide-string-prefix candidates or allow
      stronger entry evidence to override speculative interior coverage
      changes recovery policy for other games; that decision is left to
      the orchestrator.

      The caller sees a zero window handle after the skipped method and
      raises **EOSError**, message **`System Error.  Code: 126.\r\nError 126`**.
      Unwinding subsequently aborts with `SEH: unwind target has no live
      checkpoint (registration=0effff84 target=00809542 FS=0fe00000
      ESP=0efffb20)`. This is downstream of the missing method and must be
      re-evaluated after admission is fixed. No separate unwind change was
      made. The approved optional GetLogicalProcessorInformation and
      RtlCompareUnicodeString misses remain unchanged; other observed
      optional misses are not established as this failure's cause.

  Diagnostic and final verification details:

  - A temporary `RaiseException` probe saved guest memory before dispatch.
    `.venv/bin/python tools/build.py --target headless --jobs 8
    > build/task13-f15-probe-build.log 2>&1`: **exit 0**. The run command
    above, adding `RECOMP_EXCEPTION_DUMP=build/task13-exception-15.bin` and
    redirecting to `build/task13-f15-probe.log`, exited **6**. The record has
    code `0eedfade`, seven information words, and object `010f9c88` in
    `ExceptionInformation[1]`. Its VMT is `00820ce4`, its message pointer
    at object+4 is `0110eccc`, and the UTF-16 message was read using the
    length at message-4. The verified class-name pointer is at **VMT-0x38**
    (`00820d0f`, short string `EOSError`); the plan's VMT-0x2c instead points
    to code in this executable. Decoded evidence is in
    `build/task13-f15-exception.txt`. No guessed class-layout change was
    committed to the runtime.
  - All temporary probes were removed; `git -C kit diff --exit-code` passed.
    `.venv/bin/python tools/build.py --target headless --jobs 8
    > build/task13-f15-restored-build.log 2>&1`: **exit 0**.
  - Final committed-source run:
    `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-16.log 2>&1`: **exit 6**,
    with the same missing method and unwind diagnostic. Abort context:
    **EIP=0080a12a, ESP=0efffb20, EBP=0effffb4**. There are **0**
    `PeekMessageW|MsgWaitForMultipleObjectsEx` lines, **0** frame files,
    **0** oversized heap refusals, and no remaining probe output.
    The pinned executable SHA-256 was rechecked and matches
    `0c028b582632129a43ea67da6040ecc5d78a14e3bba06fcd2e4071b06a9ebd5b`.

  **Acceptance remains unmet.** The configured 600-presented-frame limit is
  never reached; Task 13 stops at finding 15's candidate-admission decision.


- **2026-09-14 — Task 13 continuation: stronger entry evidence and UTF-16 candidates.**
  Continued from kit `4f6e528` and game commit `04dfd35`, applying the
  orchestrator's finding-15 decision. Earlier findings remain above.

  15. **Translator fix — kit `be37a5a` (`Translator: let protected entries
      supersede speculative sweeps`).** Entry evidence is ranked as listed,
      explicit config/SEH or structural table seed, direct edge from a
      protected body, then scan guess. A direct edge from another scan guess
      stays weak. A newly protected target truncates weaker overlapping
      coverage before admission. A complete prefix can tail into the new
      entry; a prefix cut through an instruction is withdrawn. Removed
      instruction-interior bytes and alternate entries are cleared, while
      overlapping valid ownership is restored. A later scan guess also stops
      before already protected entries. The existing SEH split-and-adopt path
      is retained for landings into normal cleanup already attached to its
      establishing body, preserving the finding-4 RET behavior without
      promoting a speculative entry from its content.

      The subordinate recovery filter rejects speculative candidates whose
      first 16 bytes contain four consecutive printable ASCII UTF-16 pairs.
      Listed and protected entries bypass that heuristic. The private wide
      prefix fixture is promoted into `test_translate_driver.py`; independent
      non-string fixtures admit a guess before discovering a protected CALL
      target and verify truncation or withdrawal, and weak-caller fixtures
      verify that merely containing a CALL does not earn protection.

      One premise differs from the pinned PE: scanning executable sections
      found no E8/E9 rel32 transfer whose destination is `00a09a30`. The
      observed call at `00961bc6` is indirect through `[ECX+0xe4]`, with seven
      relocated vtable references to the method. The UTF-16 filter therefore
      resolves that fixture's admission; evidence ordering is independently
      verified by the non-string CALL fixtures. No new rank was invented for
      relocated pointers. The most recent missing module before the previous
      error 126 was `uxtheme.dll` (log line 1241, versus the missing method at
      line 1959); `msctf.dll` and `d3dxof.dll` were earlier misses. This does
      not establish that a new module shim is needed.

  Checks:

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k 'wide_string_prefix or protected_call_target or utf16_run_filter'
    > build/task13-f15-red.log 2>&1`: **4 failed, 1 passed, 70 deselected**,
    exit **1**, before implementation. The promoted fixture, both late-CALL
    truncation cases and speculative UTF-16 rejection fail as expected;
    the protected text-like entry already passes.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    kit/tools/recomp/tests/test_translate_insns.py
    > build/task13-f15-green.log 2>&1`: **130 passed**, exit **0**.
    An intermediate run exposed a regression in the existing cleanup-prefix
    ownership test; preserving its split-and-adopt path fixed that regression.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py
    > build/task13-f15-portable.log 2>&1`: **184 passed, 1 skipped**, exit
    **0**. The existing corpus exclusions remain. Two additional weak-caller
    parameter cases were subsequently covered by the 130-test run above.
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose
    > build/task13-f15-seh.log 2>&1`: **113 checks, 0 failures**, exit **0**,
    label `nogame`. The existing wrapper is retained because the root native
    CLI lacks `-R`.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-f15-config.log 2>&1`: **8 passed**, exit **0**.
  - `.venv/bin/python kit/tools/format.py --write
    > build/task13-f15-format.log 2>&1`: **exit 0**, 268 handwritten files
    formatted. `.venv/bin/python kit/tools/check_repo.py`,
    `.venv/bin/python kit/tools/check_game_literals.py` and the staged
    whitespace check passed before the kit commit.

  Regeneration and the next boundary:

  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f15-regenerate.log 2>&1`: **exit 1**, before native
    compilation. The final translator invariant reports **3 literal dispatch
    targets are not entry points**. A replacement translation was not
    published, so the retained binary still belongs to the preceding run.
    It was not rerun as evidence for this change.

  16. **Blocked on pruning and ownership of protected cleanup aliases; no
      fix commit.** The three failure lines are:
      - `fn_00b9c2ff dispatches to 00b9c2e9, which is not an entry point`
      - `fn_00be4cf7 dispatches to 00be4ce9, which is not an entry point`
      - `fn_00be4fd6 dispatches to 00be4fc8, which is not an entry point`

      These are Delphi HandleFinally landing JMPs to ordinary cleanup
      instructions, verified in the pinned PE. They have `seh` provenance
      and protected-entry status. At the end of discovery, `00b9c2e9` belongs
      to recovered body `00b9ba08`; both `00be4ce9` and `00be4fc8` belong to
      recovered body `00be4bf4`. Both owners have `data` provenance and are
      unprotected, consistent with finding 5. The old speculative UTF-16
      prefix at `00be4bd4` is no longer the cleanup owner; its real prologue
      is at `00be4bf4`.

      The final pruning pass withdraws `00b9ba08` because it dispatches to
      missing `00b9c307` (the real epilogue's `RET 0x10`). It withdraws
      `00be4bf4` because its CALL at `00be4e60` names missing `00919fa0`
      (a real `PUSH EBP; MOV EBP,ESP` method with its own frame). Pruning
      deletes each withdrawn owner's alternate entries, including the
      protected cleanup targets. Those targets remain in the discovery
      address set and ownership map, but are absent from the emitted entry
      table. The structurally seeded landing JMPs survive, triggering the
      final dispatch invariant. The snapshot establishes this loss; it does
      not yet establish why those two dependencies lack surviving entries.

      A bounded synthetic reproducer isolates the same pruning cascade:
      a speculative prefix with an invalid outgoing edge covers a real
      callee; a second speculative body calls that callee and establishes a
      finally cleanup. Pruning the prefix removes the callee alias, then
      prunes its caller and deletes the protected cleanup alias. The surviving
      landing JMP fails the entry-table invariant. The reproducer fails on
      both this commit and the preceding translator, establishing an existing
      gap exposed by the changed discovery results:
      - `.venv/bin/python -m pytest -q build/task13_f16_pruned_owner_test.py
        > build/task13-f16-red.log 2>&1`: **1 failed**, exit **1**, against
        restored committed source. It reports `fn_00601085 dispatches to
        00601041, which is not an entry point`.
      - `.venv/bin/python build/task13-f16-baseline.py
        > build/task13-f16-baseline.log 2>&1`: **1 failed**, exit **1**, with
        translator source loaded from kit `4f6e528` and the same fixture.

      Preserving or recovering these protected aliases needs an ownership
      rule compatible with the pushed-continuation RET behavior. Promoting a
      speculative owner simply because it contains SEH would violate finding
      5; moving cleanup into an independent body can lose finding 4's normal
      continuation. No such policy change, guest-address seed, module shim,
      or bypass of the dispatch check was made. The private reproducer remains
      ignored under `build/` for the orchestrator's next decision.

  Diagnostic and final verification details:

  - A temporary translator failure dump recorded target ownership and the
    pruning dependencies. `.venv/bin/python tools/build.py --regenerate
    --target headless --jobs 8 > build/task13-f15-diagnostic-regenerate.log
    2>&1`: **exit 1**, with the same three missing entries. The snapshot is
    `build/recomp/translate-report.json.failure.json`; the probe was removed
    afterward and `git -C kit diff --exit-code` passed. No generated code or
    runtime source was edited.
  - An initial isolated cleanup-owner hypothesis failed on the preceding
    translator too, so that attempted regression was removed. The pruning
    reproducer above captures the actual dispatch-invariant failure.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    kit/tools/recomp/tests/test_translate_insns.py
    > build/task13-f15-final-green.log 2>&1`: **130 passed**, exit **0**,
    after removal of the diagnostic probe.
  - `build/task13-f15-pe-evidence.txt` records the unchanged executable hash
    `0c028b582632129a43ea67da6040ecc5d78a14e3bba06fcd2e4071b06a9ebd5b`
    and the absence of E8/E9 rel32 references to `00a09a30`.

  **Acceptance remains unmet.** Regeneration stops before a new headless
  binary can be run. Consequently the downstream EOSError code 126 and
  no-live-checkpoint unwind failure have not been re-evaluated under this
  change, and no 600-frame/message-loop success is claimed. Task 13 stops at
  finding 16's pruning/ownership decision.
