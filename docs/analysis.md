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

- **2026-09-14 — Task 13 continued: required dispatches fall back to the
  listed span owner (finding 16).** The preceding findings remain the record
  of each earlier run. Kit commit `235b9a6` (`Translator: recover pruned
  dispatch aliases through listed spans`) applies the orchestrator's approved
  ownership rule after speculative pruning.

  16. **Cleanup aliases lost with their speculative owners — fix
      `235b9a6`.** The motivating gate lines were `fn_00b9c2ff dispatches to
      00b9c2e9`, `fn_00be4cf7 dispatches to 00be4ce9`, and
      `fn_00be4fd6 dispatches to 00be4fc8`, each reporting that the target
      was not an entry point. Pruning removed the owning speculative bodies
      and their aliases while protected landing JMPs still required them.
      A post-pruning worklist now recovers required dispatches under the
      original listed function whose span contains the target. It retains
      the next-listed-function boundary, requires clean decoding without
      overlap with the listed owner's instructions, and repeats for newly
      exposed pushed continuations, SEH entries and direct dispatches.
      Speculative owners remain pruned. Missing spans and failed decodes
      retain the dispatch gate.

      The private reproducer is promoted into `test_translate_driver.py`.
      It asserts that the cleanup wrapper belongs to the listed span owner
      after both speculative owners disappear. Variants cover a two-link
      pushed-continuation chain, a target before every listed span, and a
      malformed cleanup. The chain uses cleanup between PUSH and RET so it
      exercises span recovery independently of adjacent PUSH/RET discovery.

  Verification:

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k pruned_cleanup_alias > build/task13-f16-promoted-red.log 2>&1`:
    **2 failed, 2 passed**, exit **1**, before implementation; the missing
    alias is `fn_00601085` dispatching to `00601041`.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    kit/tools/recomp/tests/test_translate_insns.py
    > build/task13-f16-green.log 2>&1`: **134 passed**, exit **0**.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py
    > build/task13-f16-portable.log 2>&1`: **190 passed, 1 skipped**,
    exit **0**. The excluded files depend on another game's private corpus.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-f16-config.log 2>&1`: **8 passed**, exit **0**.
  - `.venv/bin/python kit/tools/format.py --write
    > build/task13-f16-format.log 2>&1`: **268 handwritten source files
    formatted**, exit **0**. `kit/tools/check_repo.py`,
    `kit/tools/check_game_literals.py`, and the staged whitespace check
    passed before the kit commit.
  - `shasum -a 256 original/gog/Siege.exe` still reports
    `0c028b582632129a43ea67da6040ecc5d78a14e3bba06fcd2e4071b06a9ebd5b`.

  Regeneration and headless verification:

  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f16-regenerate.log 2>&1`: **exit 0**. Translation took
    **517.26 seconds** and emitted **29,684 bodies / 42,649 entries**, with
    **0 failures, 0 table gaps, and 0 undecoded table sites**. It withdrew
    1,382 speculative bodies and recovered 367 required entries through the
    post-pruning span fallback. Native compilation linked `pop_headless`;
    the linker retained its existing common-section alignment warning.
    The generated wrappers confirm that `00b9c2e9` belongs to `00b9a2c8`,
    and `00be4ce9` / `00be4fc8` belong to `00be4b00`. The method
    `00a09a30` from finding 15 is now emitted too. `00919fa0` remains absent:
    its speculative caller was withdrawn, so no surviving protected dispatch
    requires it under this rule.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-17.log 2>&1`: **exit 6**.
    `RECOMP_MAX_FRAMES` is the host's actual switch, counting presented
    frames; import logging is `RECOMP_LOG=2`. The run stops at finding 17
    below, with **0 frame files**.
  - `grep -c 'PeekMessageW\|MsgWaitForMultipleObjectsEx'
    build/task13-run-17.log`: output **0**, exit **1**. The 600-frame
    message-loop acceptance remains unmet.
  - `.venv/bin/python build/task-k1-native.py seh_tests --verbose
    > build/task13-f16-seh.log 2>&1`: **113 checks, 0 failures**, exit **0**,
    label `nogame`. This existing wrapper invokes the kit's configure/build
    and test helpers; the root native CLI still lacks `-R`.

  17. **Blocked: UTF-16 scan rejection matches a zero-local prologue; no fix
      commit.** The new runtime line is `call to unknown target 00bca4c8
      (ESP=0efffb64, return=00bcebb1): returning 0`. The pinned PE contains
      a real method there: `PUSH EBP; MOV EBP,ESP`, seven `PUSH 0`
      instructions, saved registers and an SEH frame. Its first 16 bytes are
      `55 8b ec 6a 00 6a 00 6a 00 6a 00 6a 00 6a 00 6a`.
      Finding 15's filter sees consecutive printable `6a 00` UTF-16 pairs
      and rejects the speculative candidate. This is not an alignment or
      instruction-support failure: alignment is 4, the target passes the
      plausible-entry check, and a bounded sweep recovers 132 instructions
      that translate successfully. A relocated VMT slot at `00bc768c`
      points to the method, as does metadata at `00bc77b5`; pointer evidence
      remains speculative under the approved ordering. The actual caller is
      `00bcebae CALL dword ptr [EDI+0xc]`, not a direct static CALL.

      A bounded synthetic case with the same zero-local prologue and a
      relocated pointer reproduces the omission:
      `.venv/bin/python -m pytest -q build/task13_f17_push_zero_entry_test.py
      > build/task13-f17-red.log 2>&1`: **1 failed**, exit **1**, because
      `fn_00601020` is absent. The reproducer and pinned-PE evidence
      (`build/task13-f17-pe-evidence.txt`) remain ignored. No filter
      exception or stronger relocation rank was invented: admitting this
      method requires an orchestrator decision distinguishing these valid
      instruction bytes from the literal UTF-16 rejection rule.

      The subsequent line is `SEH: registration outside guest stack
      (registration=0000000d target=00000000 FS=0fe00000 ESP=0efffb28)`.
      The method's real epilogue is `RET 4` at `00bca6a0`; the unknown-call
      fallback uses a plain RET. The caller pushes its argument at
      `00bceb94`, invokes the virtual method, then restores its SEH chain
      with `POP EDX; POP ECX; POP ECX; MOV FS:[EAX],EDX`. Omitting the
      callee's four-byte cleanup leaves the argument in that first POP's
      slot, explaining the invalid registration value. The chain walker
      aborts before an unhandled exception-object report is reached. This
      downstream SEH failure should be re-evaluated after method admission.

  Error 126 re-evaluation: this run has **no GetLastError, FormatMessageW,
  or no-live-checkpoint diagnostic**, and stops before the earlier
  `uxtheme.dll` lookup. Its last missing-module line is
  `LoadLibrary("d3dxof.dll"): no shims for that module, reporting it as
  missing` at line 589, followed by hundreds of successful import calls
  before the new missing-method line at 1078. `msctf.dll` is also reported
  missing. This does not establish a need for either module's shim, nor
  establish that the previous error-126 path is fixed. The documented
  optional lookup misses remain unchanged.

  **Acceptance remains unmet.** Finding 16 is fixed and the headless build
  succeeds, but Task 13 stops at finding 17's admission-policy boundary.

- **2026-09-14 — Task 13 continued: narrow the UTF-16 guard (finding 17).**
  The orchestrator supplied two corrections to finding 15's guard. Kit
  `d5778d1` (`Translator: narrow UTF-16 rejection for compiler prologues and
  relocations`) exempts relocation-named candidates from that guard and
  excludes byte `0x6a` from its printable-ASCII set. Other admission,
  ownership and pruning rules remain as previously approved.

  17. **Repeated PUSH 0 prologue rejected as UTF-16 — fix `d5778d1`.** The
      motivating runtime line was `call to unknown target 00bca4c8
      (ESP=0efffb64, return=00bcebb1): returning 0`. Its seven `6a 00`
      instructions had matched the old guard. The private reproducer is now
      a driver regression, covering the prologue both with and without
      relocation evidence. A separate case confirms that relocation evidence
      bypasses the guard even for other printable byte pairs. The existing
      speculative `ABCD` and `MDICLIENT` rejection cases still pass.
      Direct PE checks confirm that both `00bca4c8` and `00919fa0` now fail
      the text-pattern test; the latter has no relocation reference.

  Verification:

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k 'zero_local_pushes or utf16_run_filter or wide_string_prefix or protected_call_target'
    > build/task13-f17-promoted-red.log 2>&1`: **3 failed, 7 passed**,
    exit **1**, before implementation. The failures cover both zero-local
    variants and the relocation exemption.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    kit/tools/recomp/tests/test_translate_insns.py
    > build/task13-f17-green.log 2>&1`: **137 passed**, exit **0**.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py
    > build/task13-f17-portable.log 2>&1`: **193 passed, 1 skipped**,
    exit **0**, retaining the private-corpus exclusions used earlier.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-f17-config.log 2>&1`: **8 passed**, exit **0**.
  - `.venv/bin/python kit/tools/format.py --write
    > build/task13-f17-format.log 2>&1`: **268 handwritten source files
    formatted**, exit **0**. The repository-boundary, game-literal and
    staged whitespace checks passed before the kit commit.
  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f17-regenerate.log 2>&1`: exit **0**. Translation took
    **542.6 seconds** and emitted **29,811 functions / 42,854 entries**,
    including **13,043 alternate entries**. The jump-table gate reports
    **0 entries dispatch nowhere / 0 sites decoded nothing**. The final
    link succeeded with the existing `__DATA,__common` alignment warning.
    Generated wrappers now include both `fn_00bca4c8` and `fn_00919fa0`.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-18.log 2>&1`: exit **6**,
    **0 frame files**, and **0 matches** for
    `PeekMessageW|MsgWaitForMultipleObjectsEx`. The log confirms caps of
    **600 frames / 180 seconds**. Finding 17's unknown method and invalid
    registration `0000000d` no longer appear; startup advances to the
    previously observed form-creation path.

  18. **Blocked: a relocated UTF-16 literal hides a relocated virtual
      method; no fix commit.** The run stops at line 1959:
      `call to unknown target 00a09a30 (ESP=0efffbc4,
      return=00961bcc): returning 0`. The new relocation exemption also
      admits the real `MDICLIENT` literal at `00a09a1c`: relocation slot
      `00a098e4`, in a `MOV EDX,imm32`, names that string address. The
      method at `00a09a30` is independently named by relocated vtable
      slots, including `0098a020`. A base relocation therefore identifies
      an address in both cases, without distinguishing code from text.

      Generated `body_00a09a1c` contains the speculative string decode and
      runs through `00a09b65`; its instruction at `00a09a2f` consumes
      bytes `00 55 8b`, including the real method's `PUSH EBP; MOV EBP,ESP`
      prologue. Thus `00a09a30` is rejected as an instruction-interior
      address, and no wrapper is emitted for it. The actual caller is
      virtual (`CALL [ECX+0xe4]` at `00961bc6`), so the approved stronger
      direct-CALL evidence rule does not rescue this entry. Both competing
      candidates retain speculative provenance under the current rules.

      A synthetic fixture with relocations to both the string and the
      following method reproduces the conflict:
      `.venv/bin/python -m pytest -q build/task13_f18_relocated_string_prefix_test.py
      > build/task13-f18-red.log 2>&1`: **1 failed**, exit **1**, because
      the method wrapper is absent. The same fixture against the preceding
      translator (`235b9a6`), loaded by
      `.venv/bin/python build/task13-f18-baseline.py
      > build/task13-f18-baseline.log 2>&1`, gives **1 passed**, exit **0**.
      Finding 15's existing MDICLIENT regression names only the method with
      a relocation, which explains why it still passes. These private
      reproducer files and the pinned-PE decode in
      `build/task13-f18-pe-evidence.txt` remain ignored. The executable's
      SHA-256 is unchanged. No new admission ordering or exception to the
      approved relocation exemption was invented.

  The last missing-module line before this stop is
  `LoadLibrary("uxtheme.dll"): no shims for that module, reporting it as
  missing` at line 1241; `d3dxof.dll` appears at line 589. Neither module
  was stubbed. The missing method is followed by `GetLastError`,
  `FormatMessageW`, `RaiseException`, then
  `SEH: unwind target has no live checkpoint (registration=0effff84
  target=00809542 FS=0fe00000 ESP=0efffb20)` at line 1980. This repeats the
  earlier error-126 path, whose exception was previously inspected as
  `EOSError`; the current log does not print the error value or exception
  object because it aborts during unwind before the unhandled-exception
  reporter. The remaining unknown method prevents attributing this path
  to an absent module or treating the unwind checkpoint as an independent
  defect. Optional lookup misses are unchanged.

  **Acceptance remains unmet.** Finding 17 is fixed as directed. Task 13
  stops at finding 18: the recovery rules need to resolve overlapping
  relocation-backed code and data candidates without reinstating the
  zero-local prologue rejection.

- **2026-09-15 — Task 13 continued: relocation evidence and text rejection
  (finding 18).** The orchestrator withdrew finding 17's relocation
  exemption. Kit `ec2a84b` (`Translator: rank relocated candidates without
  exempting string data`) keeps the `0x6a` exclusion and applies the UTF-16
  content filter to speculative candidates regardless of relocation.

  18. **Relocated string sweep hides a relocated method — fix `ec2a84b`.**
      The motivating line was `call to unknown target 00a09a30
      (ESP=0efffbc4, return=00961bcc): returning 0`. The promoted fixture
      names both the `MDICLIENT` literal and the following prologue with
      relocations, and now requires the literal to be rejected and the
      method emitted. The finding-17 zero-local cases remain green.

      Entry evidence now ranks original listings above explicit/structural
      seeds, protected direct edges, relocated pointers, and bare guesses,
      in that order. Relocated candidates can truncate a weaker sweep or
      bound a later one, but remain subject to content validation and
      pruning. They do not gain protected SEH provenance merely from a
      relocation. A dedicated relocation pass reaches the resolver even
      when a bare guess has covered the first byte; it preserves stronger
      instruction boundaries. Regression variants cover both discovery
      orders, aligned and crossing instructions, and listed/seeded owners.

  Verification before regeneration:

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k 'wide_string_prefix or utf16_run_filter or zero_local_pushes or relocated_method_outranks'
    > build/task13-f18-promoted-red.log 2>&1`: **4 failed, 7 passed**,
    exit **1**, before implementation. Failures cover the relocated text
    filter and both weaker-prefix overlap cases.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    kit/tools/recomp/tests/test_translate_insns.py
    > build/task13-f18-green.log 2>&1`: **151 passed**, exit **0**.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py
    > build/task13-f18-portable.log 2>&1`: **207 passed, 1 skipped**,
    exit **0**, retaining the earlier private-corpus exclusions.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-f18-config.log 2>&1`: **8 passed**, exit **0**.
  - `.venv/bin/python kit/tools/format.py --write
    > build/task13-f18-format.log 2>&1`: **268 handwritten source files
    formatted**, exit **0**. Repository-boundary, game-literal and staged
    whitespace checks passed before the kit commit.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k relocated_alias_keeps_listed > build/task13-f18-alias-red.log 2>&1`:
    **1 failed**, exit **1**, before preserving the stronger evidence of an
    already-listed instruction boundary. The case is included in the final
    passing suites above. The initial regeneration was deliberately stopped
    for this correction (exit **241**, log preserved as
    `build/task13-f18-regenerate-initial.log`); no binary from that attempt
    was used. The correction was included in the same finding-18 kit commit.
  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f18-regenerate.log 2>&1`: exit **1**, at the table gate:
    **0 dangling table entries / 3 sites decoded nothing**. The sites were
    `00a5ea5a` under `00a5e86c`, `00a60edb` under `00a60d7c`, and
    `00b5a047` under `00b597dc`. No headless run used this failed attempt;
    the previous binary and report remained in place.

  19. **Stale table-site records after ownership truncation — fix
      `d60c717` (`Translator: rebuild table-site coverage after ownership
      discovery`).** The gate reported `jump table at 00a5ea5a
      (in fn_00a5e86c) reads 00a5ea61 but decoded no entries at all`, plus
      the two analogous sites above. The pinned PE contains ordinary
      absolute tables, guarded by `CMP reg,7; JA` or `CMP reg,0xf; JA`.
      The table decoder already supports these shapes.

      Discovery recorded the switch under an earlier speculative body.
      Truncating that body transferred its suffix to a stronger entry, but
      the final pass cleared decoded tables without clearing the cached
      table-site records. The surviving prefix was therefore falsely
      reported as containing an undecoded switch. The final strict pass now
      rebuilds both collections from the final bodies. A synthetic driver
      regression reproduces the old-owner false positive; another verifies
      that a genuinely undecoded live table still fails the gate. No table
      rule or gate override was added.

      `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
      -k truncated_guess_drops > build/task13-f19-red.log 2>&1`:
      **1 failed**, exit **1**, before implementation, with the same
      `1 table sites decoded nothing` failure under the old prefix.
      `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
      kit/tools/recomp/tests/test_translate_insns.py
      > build/task13-f19-green.log 2>&1`: **153 passed**, exit **0**.
      `.venv/bin/python kit/tools/format.py --write
      > build/task13-f19-format.log 2>&1` formatted **268 handwritten source
      files**; repository-boundary, game-literal and staged whitespace
      checks passed before the kit commit. The three PE table decodes remain
      ignored in `build/task13-f19-pe-evidence.txt`.

  Final verification and run:

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py
    > build/task13-f19-portable.log 2>&1`: **209 passed, 1 skipped**,
    exit **0**, including both table-site regressions.
  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f19-regenerate.log 2>&1`: exit **0**. Translation took
    **1045.7 seconds** and emitted **29,791 functions / 42,739 entries**,
    including **12,948 alternate entries**, with **0 failures**, **0 table
    gaps**, and **0 undecoded table sites**. The headless target linked
    successfully with the existing `__DATA,__common` alignment warning.
    `fn_00a09a30`, `fn_00bca4c8` and `fn_00919fa0` are all emitted.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-19.log 2>&1`: exit **6**,
    **411 log lines**, **0 frame files**, and **0 matches** for
    `PeekMessageW|MsgWaitForMultipleObjectsEx`. The run stops at finding 20.

  20. **Blocked: an equally ranked short-string guess hides a method stub;
      no fix commit.** Line 408 is `recomp: no block entry for indirect jump
      to 0x0086e98c from 0x00809235`. The source is `JMP ESI` in listed
      `00809220`: it calls `008091f0` to look up a method through the class
      metadata and jumps to the returned address. This is an in-image method
      target, not a heap-generated thunk or an omitted SEH landing.

      The nearest listed span for the target is `0086e8e8..0086e990`, but
      that listing ends at `0086e8ee`. The PE bytes at `0086e98c` are
      `33 c0 c3` (`XOR EAX,EAX; RET`), followed by a NOP. A relocated
      metadata slot at `008460a0` names the stub. Immediately before it,
      `0086e988` holds UTF-16 `"."` and its terminator (`2e 00 00 00`),
      itself named by a relocated `PUSH` operand at `0086e947`.

      That single-character literal does not meet the four-pair string
      guard and both targets have relocation rank. The string sweep decodes
      `0086e988: 2e 00 00` as `ADD byte ptr CS:[EAX],AL`, then
      `0086e98b: 00 33` as `ADD byte ptr [EBX],DH`; this consumes the stub's
      first byte. Generated `fn_0086e988` contains those instructions, while
      `fn_0086e98c` is absent. The current ordering permits truncation only
      for strictly stronger evidence, so the relocated stub cannot displace
      this equally ranked body. The arithmetic-jump and call-return rules
      do not admit a missing method outside the dispatcher's own body.

      `.venv/bin/python -m pytest -q build/task13_f20_short_string_prefix_test.py
      > build/task13-f20-red.log 2>&1`: **1 failed**, exit **1**, because
      the relocated stub wrapper is absent. The reproducer and pinned-PE
      decode (`build/task13-f20-pe-evidence.txt`) remain ignored. No change
      to the four-pair guard, tie-breaking rule, or game-specific entry seed
      was invented; this requires an orchestrator decision.

  Module/exception re-evaluation: this run's only missing-module line is
  `LoadLibrary("msctf.dll"): no shims for that module, reporting it as
  missing` at line 307. It has no `uxtheme.dll` or `d3dxof.dll` lookup,
  `GetLastError`, `FormatMessageW`, `RaiseException`, or `RtlUnwind` call.
  It therefore does not re-establish the prior error-126/checkpoint path,
  nor prove that path fixed. No module shim was added. The optional
  `GetLogicalProcessorInformation`, `RtlCompareUnicodeString`, and
  `InitializeConditionVariable` misses remain unchanged.

  **Acceptance remains unmet.** Findings 18 and 19 are fixed and the new
  headless build succeeds. Task 13 stops at finding 20's equal-evidence
  admission boundary before reaching the VCL message loop.

- **2026-09-15 — Task 13 continued: candidate boundaries and short constants
  (finding 20).** Applied the orchestrator's boundary and terminator rules in
  kit **e23246a** (`Translator: bound speculative bodies at candidate entries`).
  Scan candidates are collected before any speculative sweep can hide another
  candidate in its instruction bytes. Their relative evidence rank does not
  permit a sweep to cross a candidate boundary. An earlier body survives only
  if its prefix terminates; NOP fallthrough and partial instructions withdraw
  it. Listed and established cleanup ownership retain their existing rules.

  20. **Fixed: the equally ranked short-string guess no longer hides the
      method stub.** The previous run stopped at line 408, `recomp: no block
      entry for indirect jump to 0x0086e98c from 0x00809235`. The adjacent
      UTF-16 `"."` guess at `0086e988` had swept across the relocated
      `XOR EAX,EAX; RET` stub at `0086e98c`. The promoted regression now
      requires the stub to be emitted. Independent equal-rank regressions
      verify a terminating JMP prefix survives with a separate callee, while
      an unterminated NOP prefix is withdrawn.

      The secondary guard validates a complete constant UnicodeString header,
      nonzero UTF-16 units, and the terminating zero word, including a
      one-character constant. **Offset correction:** the pinned PE bytes at
      `0086e97c` are `b0 04 02 00 ff ff ff ff 01 00 00 00`. Thus the word
      at `p-12` is the code page (1200), and the element-size word (2) is at
      **`p-10`**, not `p-12` as proposed. The guard and synthetic fixtures use
      this verified layout. Invalid size, reference count, length, embedded
      zero, terminator, and out-of-image length do not establish a constant.

  Verification before regeneration:

  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    -k 'short_string or equal_rank_candidates or utf16_constant'
    > build/task13-f20-tests-red.log 2>&1`: **11 failed, 100 deselected**,
    exit **1**, before implementation.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
    kit/tools/recomp/tests/test_translate_insns.py
    > build/task13-f20-green.log 2>&1`: **164 passed**, exit **0**.
    Earlier NOP-prefix expectations were updated to the newly required
    withdrawal behavior. A speculative prefix falling into a separately
    listed cleanup is likewise withdrawn; the listed cleanup survives.
    The interior-handler ownership fixture now explicitly seeds its owner,
    preserving its ownership test without relying on speculative fallthrough.
  - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
    --ignore=kit/tools/recomp/tests/test_translate.py
    --ignore=kit/tools/recomp/tests/test_eaxa.py
    --ignore=kit/tools/recomp/tests/test_translate_hooks.py
    > build/task13-f20-portable.log 2>&1`: **220 passed, 1 skipped**, exit **0**.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-f20-config.log 2>&1`: **8 passed**, exit **0**.
  - `.venv/bin/python kit/tools/format.py --write
    > build/task13-f20-format.log 2>&1`: **268 handwritten source files**,
    exit **0**, before the kit commit. `kit/tools/check_repo.py`,
    `kit/tools/check_game_literals.py`, and the whitespace check passed.

  Regeneration with finding 20's first fix:

  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f20-regenerate.log 2>&1`: exit **0**; translation took
    **283.6 seconds**, emitting **29,309 functions / 41,832 entries** with
    **12,523 alternate entries**, **0 failures**, **0 table gaps**, and
    **0 undecoded table sites**. `fn_0086e98c` is emitted and the string
    `fn_0086e988` is absent. The earlier recovered methods `00a09a30`,
    `00bca4c8`, and `00919fa0` remain emitted. Native linking succeeds with
    the existing section-alignment warning.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-20.log 2>&1`: exit **6**,
    **854 log lines**, **0 frame files**, **0 message-loop matches**. Startup
    passes the old indirect-jump stop but exposes finding 21 below.

  21. **Fixed candidate classification before applying boundaries**, kit
      **4640283** (`Translator: preserve instruction and cleanup ownership
      at scan boundaries`). The first implementation treated raw dword hits
      and callable cleanup aliases as independent function starts too early.
      Run 20 logs unknown calls to `00bfd16c` (line 84), `00bfdfb0` (308),
      `00931248` (471), and `008fe9b8` (544).

      `00bfe000` and `008fe9dc` are unrelocated scan hits inside instructions
      of the relocated routines `00bfdfb0` and `008fe9b8`, respectively.
      In the latter, the hit lies in the immediate of the `MOV EAX,0xc025dc`
      at `008fe9da`; in the former it is the second byte of `XOR EDX,EDX`
      at `00bfdfff`. Such weaker evidence must first pass instruction-boundary
      admission. The fix establishes relocated instruction coverage before
      allowing weaker raw hits to become candidate boundaries.

      The other path crossed distinct cleanup stubs to blocks of the same
      function. A single upper bound on the entire recursive descent hid
      those blocks, although no linear instruction path crossed a candidate
      entry. `00931248` was pruned for missing `0093130a`, whose three
      instructions fall into its own PUSH-named epilogue `00931315`.
      `00bfd16c` was pruned for missing `0082ebb0`, whose cleanup and
      epilogue aliases had similarly become barriers. Boundaries now stop
      each linear path, including partial opcodes, while direct branches
      may skip separate stubs. Pushed continuations and SEH landings retain
      their body ownership. An unrelated candidate in a gap between owned
      blocks no longer truncates that body. None of these probes promotes
      a speculative body to protected provenance; pruning remains active.

      Regression commands:
      - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
        -k 'bare_scan_hit_inside or speculative_body_keeps_branches'
        > build/task13-f21-red.log 2>&1`: **2 failed, 111 deselected**, exit **1**.
      - `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
        kit/tools/recomp/tests/test_translate_insns.py
        > build/task13-f21-green.log 2>&1`: **166 passed**, exit **0**.
      - `.venv/bin/python -m pytest -q kit/tools/recomp/tests
        --ignore=kit/tools/recomp/tests/test_translate.py
        --ignore=kit/tools/recomp/tests/test_eaxa.py
        --ignore=kit/tools/recomp/tests/test_translate_hooks.py
        > build/task13-f21-portable.log 2>&1`: **222 passed, 1 skipped**, exit **0**.
      - `.venv/bin/python kit/tools/format.py --write
        > build/task13-f21-format.log 2>&1`: **268 handwritten source files**,
        exit **0**, before the kit commit. Repository, literal, and whitespace
        checks passed.

      Run 20 later refuses a **547,618,816-byte** allocation (line 579), then
      reports `SEH: registration outside guest stack (registration=0080ac4b
      target=00000000 FS=0fe00000 ESP=0080ac4b)` (line 850). These follow
      the skipped methods and require re-evaluation after the admission fix.
      Its only missing module is `msctf.dll`; it has no `uxtheme.dll` or
      `d3dxof.dll` lookup. No missing-module stub was added.

  Finding 21 regeneration and run:

  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f21-regenerate.log 2>&1`: exit **0**, **411.5 seconds**
    of translation; **28,978 functions / 42,020 entries**, **13,042 alternate
    entries**, **0 failures**, **0 table gaps**, **0 undecoded table sites**.
    `00bfdfb0` and `008fe9b8` are restored; `00bfd16c` and `00931248` remain
    absent, leading to finding 22.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-21.log 2>&1`: exit **6**,
    **1,819 log lines**, **0 frame files**, **0 message-loop matches**.
    The oversized allocation and code-address stack pointer from run 20
    disappear. Startup reaches window registration, but still skips several
    missing methods and then fails to unwind to a live checkpoint.

  22. **Fixed pre-existing cleanup aliases blocking their establishing
      body's admission**, kit **ad5bbdf** (`Translator: recover owned cleanup
      before speculative admission`). Run 21 still logs missing `00bfd16c`
      (line 84) and `00931248` (470); later missing entries are `00bfe8f8`
      (613), `00bfe99c` (615), `00a0741c` (648), `00a0805c` (689),
      `008fe90c` (891), and `00a09208` (1778).

      Beyond the candidate-boundary set, the recovery routine's existing
      instruction-owner stop set could end a prefix at an already recovered
      cleanup. Applying the terminator rule at that point rejected the
      establishing body before ordinary cleanup adoption could run.
      Recovery now follows its own PUSH-named continuations and SEH landing
      blocks within the containing span to a fixed point first. Only clean
      fragments are excluded from the stop/boundary sets. Ordinary adoption
      then transfers ownership; the entry stays speculative and prunable.

      `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
      -k preexisting_cleanup > build/task13-f22-red.log 2>&1` produced
      **1 failed, 113 deselected**, exit **1**, before implementation.
      The fixture seeds the cleanup through another listed frame before
      discovering its establishing body as a scan candidate.
      `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
      kit/tools/recomp/tests/test_translate_insns.py
      > build/task13-f22-green.log 2>&1` now gives **167 passed**, exit **0**.
      The original cleanup-owner assertions and unseeded interior-handler
      fixture are restored: those paths are established cleanup ownership,
      not unterminated fallthrough into an unrelated candidate.
      `.venv/bin/python -m pytest -q kit/tools/recomp/tests
      --ignore=kit/tools/recomp/tests/test_translate.py
      --ignore=kit/tools/recomp/tests/test_eaxa.py
      --ignore=kit/tools/recomp/tests/test_translate_hooks.py
      > build/task13-f22-portable.log 2>&1` gives **223 passed, 1 skipped**,
      exit **0**. Formatting again covered **268 handwritten sources**;
      repository, literal, and whitespace checks passed before the commit.

      Run 21's last missing-module lookup is the expected `uxtheme.dll`
      miss at line 1142; `d3dxof.dll` is also correctly missing at line 588.
      `GetLastError`, `FormatMessageW`, and `RaiseException` follow window
      registration at lines 1796–1800, then line 1816 reports `SEH: unwind
      target has no live checkpoint (registration=0effff84 target=00809542
      FS=0fe00000 ESP=0efffb20)`. Missing methods still precede this path,
      so neither module is stubbed and exception causality remains pending
      the next run.

  Finding 22 regeneration and run:

  - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
    > build/task13-f22-regenerate.log 2>&1`: exit **0**, **464.0 seconds**
    of translation; **29,174 functions / 42,112 entries**, **13,100 alternate
    entries**, and **0 failures / table gaps / undecoded table sites**.
    `00931248` and `00a0741c` are restored.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-22.log 2>&1`: exit **6**,
    **1,951 lines**, **0 frames / message-loop matches**. The log still skips
    `00bfd16c`, `00bfe8f8`, `00bfe99c`, `00a0805c`, `008fe90c`, and `00a09208`.
    Following the last miss (line 1907), window creation receives garbled text
    and dimensions, and `CallWindowProcW` attempts `0087741e` (1919). That is
    the return address after the buffer-copy CALL at `00877419`, not a new
    callback. The run ends with the old unwind-checkpoint failure (1948).

  23. **Fixed bare pointers to a relocated method's RET suppressing the
      method**, kit **779d278** (`Translator: keep weaker instruction pointers
      as body aliases`). Run 22's missing `00bfe8f8` (616) has a bare scan hit
      at its RET, `00bfe8ff`; `008fe90c` (937) likewise has a hit at `008fe91f`.
      These weaker pointers name instruction aliases within the stronger
      relocated body. Treating them as independent function boundaries cut
      the methods one instruction before their terminating RET. They now
      remain callable aliases, and relocated candidates are resolved before
      immediate guesses. Equal-rank boundaries remain intact.

      `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
      -k bare_pointer_to_relocated > build/task13-f23-red.log 2>&1` gave
      **1 failed, 114 deselected**, exit **1**. The focused driver/instruction
      command gave **168 passed**, exit **0**, in `build/task13-f23-first.log`.
      The same portable command used for finding 22 gave **224 passed,
      1 skipped**, exit **0**, in `build/task13-f23-portable.log`.
      `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
      > build/task13-f23-config.log 2>&1` gave **8 passed**, exit **0**.
      Formatting covered **268 handwritten sources** and the repository,
      literal, and whitespace checks passed before committing.

      Regeneration (`build/task13-f23-regenerate.log`, same build command)
      exited **0**: **473.9 seconds**, **29,165 functions / 42,572 entries**,
      **13,547 alternate entries**, and all three translation gates at **0**.
      The same headless command with `> build/task13-run-23.log 2>&1` exited
      **139** (SIGSEGV), with **675,213 lines**, **0 frames / message-loop
      matches**. Only `00bfd16c` remains an unknown call (line 84); the other
      omitted methods and corrupted callback pointer from run 22 disappear.
      Form initialization and GDI calls now proceed until
      `LoadLibrary("msimg32.dll"): no shims for that module` (675180), followed
      by `RaiseException` (675182), a dispatched unwind (675185), and
      `MessageBoxW: External exception C06D007E.` (675212). The old missing
      checkpoint diagnostic is absent. The PE delay-imports **GradientFill**
      and **AlphaBlend** from that module; neither exists in the kit. Recheck
      after the remaining initialization callback is restored before deciding
      the next blocker. Expected `d3dxof.dll` and `uxtheme.dll` misses remain.

  24. **Fixed a PUSH-derived span fragment inheriting its listed owner's
      protection**, kit **9e621ae** (`Translator: retain provenance of guessed
      span fragments`). The remaining `00bfd16c` miss calls `0082ebb0`.
      `0082eac4` pushes `0082ebac` as a headerless UTF-16 `"\\"` API argument.
      The continuation heuristic had decoded this data as `POP ESP; ADD ...`,
      misaligned the real prologue at `0082ebb0`, and attached the resulting
      instructions to the listed owner. Neither string guard matches this
      short headerless constant. The approved provenance and boundary rules
      therefore apply to the guessed fragment itself: record its origin,
      withdraw its unterminated prefix when a later CALL identifies the
      method, and preserve the original listing and unrelated fragments.
      Future span sweeps honor known candidate boundaries; the withdrawn
      data fragment cannot be reintroduced by the fallback pass.

      `.venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py
      -k pushed_data_fragment > build/task13-f24-red.log 2>&1` produced
      **1 failed, 115 deselected**, exit **1**, before implementation. The
      synthetic fixture discovers the method through a relocated callback
      after the bad fragment was already adopted. The focused driver and
      instruction suites (`build/task13-f24-green.log`) give **170 passed**,
      exit **0**; the portable suites (`build/task13-f24-portable.log`) give
      **226 passed, 1 skipped**, exit **0**. The config/literal command above,
      logged to `build/task13-f24-config.log`, gives **8 passed**, exit **0**.
      Formatting covered **268 handwritten sources**, and repository,
      literal, and whitespace checks passed before the commit.

      A second regression, `-k span_fragment_keeps`, initially produced
      **1 failed, 116 deselected** (`build/task13-f24-alias-red.log`): a
      scanned alias must not split a fragment's own PUSH-named RET epilogue.
      That ownership guard is included in the final finding 24 commit and
      the passing counts above. The preliminary regeneration was stopped
      with SIGTERM before completion (wrapper exit **241**), retained as
      `build/task13-f24-prereview-regenerate.log`, and restarted from the
      corrected commit. No preliminary binary was reported as a passing run.

      The fixture also now rejects any RET-switch label for the withdrawn
      literal. That assertion failed before rebuilding the owning function's
      cached address/fallthrough metadata (`build/task13-f24-metadata-red.log`,
      **1 failed, 116 deselected**, exit **1**). The correction is included in
      the same final commit; the full focused and portable counts above still
      pass. A second obsolete regeneration was stopped before compilation
      (exit **241**, `build/task13-f24-metadata-prereview-regenerate.log`).

      The completed finding 24 regeneration
      (`build/task13-f24-regenerate.log`, same build command) exited **0**:
      **474.1 seconds**, **29,237 functions / 42,602 entries**, **13,505
      alternate entries**, **0 failures / table gaps / undecoded table sites**.
      However, inspecting the generated output showed that `0082ebb0` and
      `00bfd16c` were still absent. The bad `0082ebac` prefix had moved from
      the listed owner to a standalone speculative body. The same headless
      command (`build/task13-run-24.log`) again exited **139**, with **675,213
      lines**, **0 frames / message-loop matches**, and the same unknown
      initializer, `msimg32.dll` delay-load exception, and final SIGSEGV as
      run 23. The passing small fixture did not yet cover that SEH path.

  25. **Fixed cleanup ownership and scan readmission after span withdrawal**,
      kit **c7ba575** (`Translator: retain cleanup ownership when withdrawing
      span guesses`). Run 24 still reports `call to unknown target 00bfd16c`
      at line 84. Adding the omitted method's SEH shape to the fixture revealed
      the remaining path: its cleanup target triggered premature withdrawal
      of the guessed fragment, before the method's CALL was discovered. The
      later immediate/data scan then readmitted that same bad prefix as a
      standalone body. Existing `finally_owners` and SEH cleanup claims now
      exempt that target from fragment splitting, just as they already do
      for ordinary speculative bodies. A withdrawn span prefix cannot be
      readmitted by weaker pointer evidence. The real CALL can then displace
      the bad prefix and recover the method with its cleanup intact.

      The fixture is parameterized over both an SEH-containing method and a
      relocated literal. `.venv/bin/python -m pytest -q
      kit/tools/recomp/tests/test_translate_driver.py -k pushed_data_fragment
      > build/task13-f25-red.log 2>&1` initially produced **2 failed, 2 passed,
      116 deselected**, exit **1**. The focused driver/instruction command
      (`build/task13-f25-green.log`) now gives **173 passed**, exit **0**;
      the portable command with the same three private-corpus exclusions
      (`build/task13-f25-portable.log`) gives **229 passed, 1 skipped**, exit
      **0**. Formatting covered **268 handwritten sources**, and repository,
      literal, and whitespace checks passed before committing. The module
      exception remains pending re-evaluation after regeneration.

      Finding 25 regeneration and final run:

      - `.venv/bin/python tools/build.py --regenerate --target headless --jobs 8
        > build/task13-f25-regenerate.log 2>&1`: exit **0**, **478.8 seconds**
        of translation; **29,181 functions / 42,548 entries**, **13,506
        alternate entries**, and **0 failures / table gaps / undecoded table
        sites**. The generated code now contains both `fn_0082ebb0` and
        `fn_00bfd16c`; the erroneous `0082ebac POP ESP` fragment is absent.
        Native linking succeeds with the existing `__common` alignment
        reduction warning.
      - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
        RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
        build/recomp/pop_headless > build/task13-run-25.log 2>&1`: exit **5**,
        **675,214 lines**, **0 frame files**, and **no unknown call/jump
        targets**. The previously observed unknown-target diagnostics are
        absent from this run.
      - `grep -c 'PeekMessageW\|MsgWaitForMultipleObjectsEx'
        build/task13-run-25.log`: **0**, grep exit **1** (no matches).
        The configured frame cap is **600**, but neither the message loop
        nor presented frames were reached. **Task 13 acceptance is not met.**
      - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
        > build/task13-f25-config.log 2>&1`: **8 passed**, exit **0**.

  26. **New design boundary: the `msimg32.dll` drawing path is reached.**
      Run 25 line **675178** reports `LoadLibrary("msimg32.dll"): no shims
      for that module, reporting it as missing`. `GetLastError` and
      `RaiseException` follow at **675179–675180**, a dispatched unwind is
      logged at **675183**, and line **675210** displays
      `MessageBoxW: External exception C06D007E. -> default button 1`.
      The pinned PE's delay table names **GradientFill** (IAT `00c36328`,
      guest thunk `00816a90`) and **AlphaBlend** (IAT `00c3632c`, guest thunk
      `008168b0`). Neither export is implemented in the kit. This requires
      a new graphics-shim implementation decision; no placeholder module or
      success-only export was added. The earlier instructions specifically
      require reporting a newly needed module rather than inventing a stub.

      After the exception UI and `SetActiveWindow`, line **675213** reports
      `[host] SIGSEGV in guest thread 1: EIP=0effff7c ESP=0effff58 EBP=0080959b`.
      The host exits **5**; the earlier `SEH: unwind target has no live
      checkpoint` diagnostic is absent. The cause of this subsequent fault
      is not established by the missing-module log and must be re-evaluated
      after the drawing dependency is addressed. It is not an unhandled
      `0eedfade` raise; the external-exception message is intact.

      Expected `d3dxof.dll` and `uxtheme.dll` misses remain at lines **589**
      and **1241**, and startup passes both. The earlier `msctf.dll` miss
      also passes. Optional `GetLogicalProcessorInformation`,
      `RtlCompareUnicodeString`, `InitializeConditionVariable`, and
      `DirectXFileCreate` misses remain unchanged. There are no oversized
      heap refusals in the final run. The executable hash was rechecked as
      `0c028b582632129a43ea67da6040ecc5d78a14e3bba06fcd2e4071b06a9ebd5b`.
      Stop here for the new module/graphics decision; no success claim,
      other plan task, or platform work is included.

- **2026-09-15 — Task 13 continued: msimg32 drawing exports (finding 26).**
  The approved module implementation is kit commit **`32b98ba`**
  (`Runtime: implement msimg32 gradients and transparent blits`). Earlier
  findings above remain the history of their respective runs.

  26. **Fixed: msimg32 delay-load registration and raster operations.**
      `runtime/msimg32.cpp` registers `GradientFill` (6 stdcall arguments),
      `AlphaBlend` and `TransparentBlt` (11 each) through `imports_init`.
      Gradients use the shared GDI origin, clip and surface/DIB write path;
      rectangle modes interpolate across exclusive right/bottom bounds,
      and triangle mode interpolates at pixel centers. Blits scale by
      nearest neighbor, preserve premultiplied source alpha for source-over,
      and compare color keys independently of alpha. Source samples are
      captured before writes. Normal GDI reads remain opaque; an explicit
      pixel-reader option preserves 32-bit BI_RGB alpha for these operations.
      No translator change or regeneration was needed.

      New import-level tests first failed because none of the three exports
      resolved (**24 failed checks**). They now verify the 8x1 gradient
      endpoints/midpoint, vertical and triangle interpolation, origin/clip,
      half-alpha red over blue, combined constant/per-pixel alpha, constant-only
      alpha, scaled color-key copies, bad mesh indices and DCs without storage.
      The call helper verifies stdcall cleanup. There is no delay-import
      expectation list in `runtime_tests` to extend. Microsoft's
      [GradientFill](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-gradientfill),
      [BLENDFUNCTION](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-blendfunction)
      and [TransparentBlt](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-transparentblt)
      documentation informed the layout and alpha handling. The API pages
      specify TRUE/FALSE, without a specific last-error guarantee for absent
      storage; the shim explicitly reports `ERROR_INVALID_HANDLE` (6) there
      and `ERROR_INVALID_PARAMETER` (87) for invalid parameters.

      Clean run `build/task13-run-26-final.log` line **675178** now reports
      `LoadLibrary("msimg32.dll") -> pseudo module 60010000`, followed by
      `GetProcAddress`. The external exception `C06D007E` is absent. This
      proves startup passes the module lookup; the native GDI suite, rather
      than an observed game `GradientFill` trace, proves the raster outputs.

  27. **New finding: display enumeration rejects an uninitialized dmSize.**
      Clean run line **675975** calls `EnumDisplaySettingsW`, **675976**
      loads a resource string, **675977** raises, and **676002** displays
      `MessageBoxW: List index out of bounds (0) -> default button 1`.
      Temporary runtime diagnostics in `build/task13-run-27-diagnostic.log`
      line **675976** record device `\\.\DISPLAY1`, mode **0**, output
      **`0efff9d8`**, and **dmSize=4**. `enum_settings` requires dmSize >= 220
      and returns FALSE before filling the record. Its mode source is not
      reached. The pinned PE's `00bed750` function reserves a 220-byte
      DEVMODEW at EBP-0xdc but does not initialize it before the call at
      **`00bed889`**. The empty resolution list is then indexed at zero by
      **`00bed8f5`**, returning to **`00bed8fa`**; that return address appears
      in the exception's diagnostic frame chain, followed by `00bec8fc`,
      `00a063d3`, `00a05fdf`, `00a05f90`, and `00bfeaae`.

      This is not evidence that a particular resolution is missing.
      [Microsoft's caller contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaysettingsw)
      requires initialization of dmSize. In contrast,
      [Wine's implementation](https://github.com/wine-mirror/wine/blob/master/dlls/win32u/sysparams.c)
      writes the legacy DEVMODEW prefix through dmDisplayFrequency and sets
      dmSize to the dmICMMethod offset, without checking the incoming size.
      No Windows execution was available to establish behavior for this
      malformed input. A compatibility rule for uninitialized dmSize and
      the supported output extent remains open; no permissive workaround
      or fabricated mode list was committed.

  28. **New dispatch-policy boundary: computed return is also an entry.**
      The subsequent stack-address fault persists after msimg32 loads:
      clean run line **676005** reports
      `SIGSEGV in guest thread 1: EIP=0effff7c ESP=0effff58 EBP=0080959b`.
      The handler is reached through a live SEH checkpoint; there is no
      missing-checkpoint diagnostic or unhandled exception object.
      Instrumented run line **675989** records landing **`00a063e2`**,
      registration **`0effff30`**, ESP **`0efff8e4`**, EBP **`0effff4c`**.
      Line **676038** records the landing returning with EIP **`0effff7c`**,
      ESP **`0effff6c`**, EBP **`0080959b`**, before the final host fault.

      The generated `fn_00a063e2` calls DoneExcept at **`00a063f7`**, pushes
      the correctly decoded return **`00a063fc`**, then falls through to
      `CALL_FN(00a063fc)`. DoneExcept (`0080a480`) pops that return into EDX,
      restores the saved guest stack and ends in `JMP EDX` at **`0080a4ad`**.
      **`00a063fc` appears in both the entry table and the call-return table.**
      Generated `recomp_jump` follows the approved finding-3 rule: dispatch
      an entry first, and consult call returns only on an entry miss. Thus
      it executes this epilogue in a nested host frame; on returning,
      `fn_00a063e2` executes the same epilogue again. This is the generated
      control-flow explanation for the corrupted return state, not a
      misdecoded CALL or another speculative-owner pruning problem.

      The existing driver test
      `test_computed_returns_use_sorted_call_continuations` explicitly
      requires entry-first ordering and still passes. Ignored reproducer
      `build/task13_f28_call_return_entry_test.py` supplies a synthetic CALL,
      a separately admitted INC/RET continuation, and POP EDX/JMP EDX;
      it proves the continuation occupies both tables and fails the proposed
      call-return-first assertion (**667 < 140** is false). Choosing whether
      to prioritize every call return or recognize only the active caller's
      continuation changes an approved runtime rule and needs a new decision.
      The test is deliberately not added to the committed passing suite.

  **Validation and retained artifacts (all commands from the game root):**

  - `.venv/bin/python build/task-k1-native.py gdi_tests --verbose
    > build/task13-f26-gdi-red.log 2>&1`: exit **1**, **85 checks, 24 failures**,
    CTest `0% tests passed, 1 tests failed out of 1` (before implementation).
  - The same command to `build/task13-f26-gdi-green.log` and, after formatting,
    `build/task13-f26-gdi-final.log`: exit **0**, **73 checks, 0 failures**,
    `100% tests passed, 0 tests failed out of 1`.
  - `.venv/bin/python build/task-k1-native.py runtime_tests --verbose
    > build/task13-f26-runtime.log 2>&1`: exit **0**, **857 checks, 0 failures,
    1 skipped**, `100% tests passed, 0 tests failed out of 1`. The skip is
    the data-import assertion because this image imports no data symbols.
  - `.venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py
    > build/task13-f26-python.log 2>&1`: exit **0**, **8 passed in 0.06s**.
  - `.venv/bin/python kit/tools/format.py --write
    > build/task13-f26-format.log 2>&1`: exit **0**, `Formatted 269 handwritten
    source files`. `kit/tools/check_repo.py` passed tracked-source boundaries
    and documentation links; `kit/tools/check_game_literals.py` and
    `git -C kit diff --cached --check` exited **0** before the kit commit.
  - `.venv/bin/python tools/build.py --target headless --jobs 8
    > build/task13-f26-headless.log 2>&1`: exit **0**, linked `pop_headless`.
    After temporary diagnostics were removed, the same command to
    `build/task13-f26-clean-headless.log` also exited **0**. Both retained
    the pre-existing linker alignment warning. Native helper use is the
    existing adaptation for the root test wrapper's unsupported `-R` flag;
    it calls the kit configure/build/test functions without invoking a
    compiler directly.
  - `RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1
    RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames
    build/recomp/pop_headless > build/task13-run-26-final.log 2>&1`:
    exit **5**, **676,006 lines**, **25 PeekMessageW matches**, **0 frame
    files**, ending in the SIGSEGV quoted above. The first run with the same
    switches (`build/task13-run-26.log`) had the same result. The instrumented
    run also exited **5**; its temporary source patch is retained only in
    `build/task13-f27-diagnostics.patch`. The kit worktree and final headless
    binary contain no temporary diagnostic edits.
  - `.venv/bin/python -m pytest -q
    kit/tools/recomp/tests/test_translate_driver.py::test_computed_returns_use_sorted_call_continuations
    > build/task13-f28-existing-policy.log 2>&1`: exit **0**, **1 passed in 0.03s**.
  - `.venv/bin/python -m pytest -q build/task13_f28_call_return_entry_test.py
    > build/task13-f28-policy-red.log 2>&1`: exit **1**, **1 failed in 0.09s**
    at the conflicting dispatch-priority assertion, after table membership
    and CALL return-address assertions pass. No translator changes were made.

  PE decodes and the verified executable SHA-256 are retained in
  `build/task13-f27-f28-pe-evidence.txt`; the hash remains
  `0c028b582632129a43ea67da6040ecc5d78a14e3bba06fcd2e4071b06a9ebd5b`.
  Expected `d3dxof.dll` and `uxtheme.dll` misses still pass, as does the
  earlier `msctf.dll` miss. The four optional export misses are unchanged.
  There are no unknown call/jump targets or oversized heap refusals.
  **Task 13's 600-frame, exit-0 acceptance remains unmet.** The 25 message
  polls are startup processing, not proof of a sustained TApplication.Run
  loop. Stop at the new compatibility/dispatch decisions, with no changes
  to other tasks, game configuration, assets or platforms.


- **2026-09-15 — Task 13 continued: display enumeration and popped returns
  (findings 27–28).** The approved fixes are kit commits **`853802b`**
  (`Runtime: enumerate display modes with uninitialized DEVMODE sizes`) and
  **`6c12096`** (`Translator: return directly through a proven popped continuation`).
  Earlier findings and their run results above remain unchanged.

  27. **Fixed: mode zero accepts uninitialized dmSize.** The previous run's
      `EnumDisplaySettingsW` / `List index out of bounds (0)` sequence came
      from rejecting the caller's dmSize=4 before returning a mode. The shim
      already read offset **68**, so this was not an ANSI-offset mistake.
      Enumeration now fills exactly **220 bytes** for W, sets dmSize=220 and
      dmDriverExtra=0, and accepts any incoming size. The previously absent
      ANSI export shares the mode source, fills **156 bytes**, and sets its
      corresponding header fields. Both support mode zero, current (-1) and
      registry (-2); mode one ends enumeration. Invalid guest buffers still
      fail without being written.

      The decision's proposed W display offsets were corrected: bpp/width/
      height/flags/frequency are **168/172/176/180/184**, not the ANSI
      **104/108/112/116/120**. DEVMODEW widens both dmDeviceName and dmFormName;
      see [the Microsoft structure definition](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-devmodew).
      The pinned guest reads width at EBP-0x30 relative to its EBP-0xdc
      record, confirming offset **172**. Runtime tests cover sizes 0, 4 and
      the standard size for each encoding, dimensions, driver-extra zero,
      current/registry queries, termination, and a sentinel beyond the record.

  28. **Fixed statically: a JMP through the popped caller return bypasses
      entry dispatch.** The translator tracks ESP relative to entry, an EBP
      frame offset for LEAVE, and registers populated by a 32-bit POP at
      delta zero. Register writes (including partial and implicit writes)
      invalidate the corresponding fact; a later stack restore does not
      invalidate a saved return register. CALL has the approved net-zero
      stack effect. Control-flow joins retain agreeing facts, alternate
      entries begin independent call frames, and unmodelled instructions or
      unproven indirect destinations conservatively discard facts.

      A proven JMP emits `c->eip = c->r[reg]; return;` without another pop,
      lookup or dispatch. Runtime `recomp_jump` remains entry-first. The
      synthetic driver regression includes a CALL continuation admitted as
      a separate entry and a cleanup CALL after POP EDX. The instruction
      regression compares POP EDX / ADD ESP,8 / JMP EDX against Unicorn with
      two arguments. That oracle case already passed via the old fallback;
      the driver assertions exposed the double-dispatch defect. An older
      short-jump/except regression now expects the static return while
      retaining its CALL-length and call-return-table assertions.
      Direct decoding of the hash-verified PE confirms **`0080a4ad`** is the
      proven return in **`0080a480`**; evidence is in ignored
      `build/task13-f28-decode.txt`.


      Regeneration succeeded in **478.2 seconds**: **29,042 functions**,
      **42,548 entries**, and **zero jump-table entries dispatching nowhere**.
      Generated `fn_0080a480` now emits the direct return at `0080a4ad`.
      The first run with both fixes (`build/task13-run-28.log`) executed
      **1,056,525 PeekMessageW calls** for **180.0 seconds**, then handled
      the wall-clock close and called **ExitProcess(0)**. No stack-address
      SIGSEGV, missing checkpoint, or unknown-target abort occurred. The
      host process nevertheless returned **1**, because **zero frames**
      were presented. The list error still appeared at lines **676003**
      and **676272**, which led to the next finding.

  29. **Fixed: display enumeration exposed only the desktop mode rather
      than all supported modes.** In `task13-run-28.log`, lines **675975–
      675976** enumerate mode zero and then stop; the list error follows.
      The listing at `00bed7b0` through `00bed813` filters fullscreen modes
      to **800x600**, **1280x720**, or **1920x1080**. The shim's sole desktop
      fallback, **1024x768x32**, matches none. The DirectDraw shim already
      offers all three sizes through its existing supported-mode table.

      Kit commit **`6e755d5`** (`Runtime: enumerate supported display modes
      through the DirectDraw table`) shares that table with indexed ANSI
      and wide display queries. Current/registry requests still query the
      active mode or desktop fallback. No new mode list or game-specific
      exception was invented; `RECOMP_DDRAW_MODES` and the runtime mode
      setter remain the single source of offered modes. Runtime-only
      binaries retain their one-mode fallback through a weak default.
      This follows the approved rule to stop only past the last offered
      mode and the [indexed API contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaysettingsw).

      The regression uses a controlled one-mode list for the dmSize checks,
      then a two-mode list and verifies both encodings enumerate each size
      and terminate. It failed **10 checks** before the implementation;
      afterward the runtime suite passes **877 checks**, the GDI suite
      **73 checks**, and DirectX **138,819 checks**. This was a runtime-only
      rebuild; the translation from finding 28 was retained.

  30. **New boundary: the startup-settings modal loop has no headless
      presentation path.** In `build/task13-run-29.log`, line **1960** creates
      `tfrmlaunchsetting`, title **SoAOS Startup Settings**. Lines **675975–
      675989** enumerate the supported modes, and line **676165** creates a
      control displaying **800 x 600 (Original)**. The two list exceptions
      are absent. This is launcher message processing, not proof that the
      main game's TApplication.Run or main menu has been reached.

      The pinned PE resolves the virtual call at `00bfeac8` through VMT
      **`00beb4b4 + 0x168`** to **`00a0c690`**. That modal routine repeatedly
      calls **`00a11490`** at **`00a0c83a`** while its modal result at **+0x2ec**
      remains zero. `00a11490` calls `00a11364`, which retrieves messages at
      `00a11378` and `00a113af`. The later game-entry work waits for the
      startup form to finish.

      Source and binary inspection expose two presentation limitations:
      `ShowWindow`/`InvalidateRect` mark an update pending, but `peek_message`
      only drains the existing queue and does not synthesize WM_PAINT for
      dirty windows. The run has no BeginPaint/EndPaint calls. Separately,
      `nm -m build/recomp/pop_headless` reports **weak external
      _host_display_present_window**: headless links the no-op default in
      `runtime/mods_seam.cpp`, while the GDI presenter in `host/present.cpp`
      is linked into other hosts. Its frame counter measures actual presents,
      not message polls. Making this launcher draw/count frames, or choosing
      an input action to leave it, needs a new host/paint/launcher decision;
      no frame-counter substitution or automatic launcher action is added.

      The final run executes **1,171,381 PeekMessageW calls** and **11
      EnumDisplaySettingsW calls** (ten modes and the terminating query),
      with no RaiseException, list error, unknown-target abort, or
      BeginPaint/EndPaint call. At line **42258605** the host posts WM_CLOSE
      after **180.0 seconds / zero frames**. Line **42259725** records
      **ExitProcess(0)**; the host summary confirms zero presented frames
      at **42259731** and guest exit code zero at **42259748**. The process
      exit status is **1**, the headless host's no-presentation outcome.
      The streaming count and selected lines are saved in ignored
      `build/task13-run-29-summary.txt`.

      Optional misses remain `GetLogicalProcessorInformation`,
      `RtlCompareUnicodeString`, `InitializeConditionVariable`, and
      `DirectXFileCreate`; missing modules remain `msctf.dll`, `d3dxof.dll`,
      and `uxtheme.dll`. Startup passes all of these without an exception.

  Validation for this continuation (commands run from the game root;
  output files are ignored):

  ```sh
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task13-f27-runtime-red.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task13-f27-runtime-green.log 2>&1
  .venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py -k 'popped_return' > build/task13-f28-driver-red.log 2>&1
  .venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py -k 'Popped' > build/task13-f28-insns-before.log 2>&1
  .venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_driver.py > build/task13-f28-driver-green.log 2>&1
  .venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py > build/task13-f28-insns-green.log 2>&1
  .venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py > build/task13-f28-config.log 2>&1
  .venv/bin/python build/task-k1-native.py seh_tests --verbose > build/task13-f28-seh.log 2>&1
  .venv/bin/python tools/build.py --regenerate --target headless --jobs 8 > build/task13-f28-regenerate.log 2>&1
  RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1 RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames build/recomp/pop_headless > build/task13-run-28.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task13-f29-runtime-red.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task13-f29-runtime-green.log 2>&1
  .venv/bin/python build/task-k1-native.py gdi_tests --verbose > build/task13-f29-gdi.log 2>&1
  .venv/bin/python build/task-k1-native.py dx_tests --verbose > build/task13-f29-dx.log 2>&1
  .venv/bin/python tools/build.py --target headless --jobs 8 > build/task13-f29-headless.log 2>&1
  RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1 RECOMP_PROFILE_DIR=build/task13-profile RECOMP_FRAMES=build/task13-frames build/recomp/pop_headless > build/task13-run-29.log 2>&1
  ```

  - Finding 27 runtime red: **875 checks, 13 failures, 1 skipped**, exit 1;
    green: **869 checks, 0 failures, 1 skipped**, exit 0.
  - Finding 28 driver red: **7 failed, 8 passed, 120 deselected**, exit 1.
    The Unicorn reproducer already passed: **2 passed, 52 deselected**.
    Final full suites: driver **135 passed**, instruction **54 passed**,
    game config/literal checks **8 passed**, all exit 0.
  - SEH native: **113 checks, 0 failures**, CTest **100% tests passed**,
    exit 0. Finding 29 runtime red: **877 checks, 10 failures, 1 skipped**,
    exit 1; green: **877 checks, 0 failures, 1 skipped**, exit 0.
    GDI **73 checks, 0 failures** and DirectX **138819 checks, 0 failures**,
    both CTest **100% tests passed**, exit 0. The runtime skip is the
    existing image-import check when this image has no imported data symbols.
  - Both headless builds exit 0 and link `pop_headless`; both retain the
    existing linker warning reducing `__DATA,__common` alignment from
    `0x8000` to `0x4000`. Only finding 28 required regeneration. Both runs
    exit 1 at the host level, with guest ExitProcess(0) after the wall-clock
    cap, as detailed above.
  - Before each kit commit, `.venv/bin/python kit/tools/format.py --write`
    formatted **269 files**; `.venv/bin/python kit/tools/check_repo.py`,
    `.venv/bin/python kit/tools/check_game_literals.py`, and the staged
    whitespace check passed. Outputs are in `build/task13-f27-*`,
    `build/task13-f28-*`, and `build/task13-f29-*` logs.

  The existing `build/task-k1-native.py` helper is retained because the root
  test wrapper has no `-R` option. It delegates native configuration/builds
  through the kit test/build tools and runs the named CTest suite; no compiler
  is invoked directly. The actual switches are **RECOMP_MAX_FRAMES=600** and
  **RECOMP_LOG=2**, not the plan's guessed switch names. MAX_FRAMES counts
  presented frames, and the unchanged default MAX_SECONDS=180 ends these
  runs. No platform, game config, asset, or optional-module stub was added.

  **Task 13 remains incomplete.** The corrected runtime now services the
  startup-settings modal message loop without the previous exceptions and
  stack fault. It has not demonstrated the main TApplication.Run loop,
  600 presented frames, or headless process exit 0. Stop at finding 30's new
  painting/presentation/launcher boundary and record the kit pointer with
  this partial outcome rather than using the plan's success claim.


- **2026-09-15 — Task 13 continued: the first live milestone, paint synthesis
  and headless window capture (finding 30).** The orchestrator accepts run 29
  above as the **first live milestone**: the guest survives the full 180-second
  cap in its startup settings form's modal loop, correctly waiting for a click.
  The runtime must not dismiss that form; Task 14's smoke script owns input.
  The revised Task 13 acceptance is **600 presented startup-form frames and
  headless exit 0 before the wall-clock cap**, with message-loop import evidence.

  30. **Implemented the two approved gaps.** Kit **`f1c951c`** (`Runtime:
      synthesize paint messages from visible update regions`) makes PeekMessage
      and GetMessage synthesize WM_PAINT after matching queued messages and
      timers. The region remains pending under both PM_NOREMOVE and PM_REMOVE
      until BeginPaint or default paint handling validates it. Hidden windows
      and hidden child hierarchies do not synthesize paint; resize invalidation
      and synchronous UpdateWindow use the same pending region.

      The new runtime checks failed **5 checks** before the implementation and
      passed afterward: **887 checks, 0 failures, 1 skipped**. Existing tests
      asserting no additional WM_SIZE now filter for that message, and a queue
      drain dispatches paints so they can validate their regions. The WM_PAINT
      validation behavior follows [Microsoft's message documentation](https://learn.microsoft.com/en-us/windows/win32/gdi/wm-paint).
      The approved timer-before-paint ordering is retained for this kit's queued
      timers; Microsoft's PeekMessage documentation lists generated WM_PAINT
      before generated WM_TIMER in the default Windows ordering.

      Kit **`c0aff3a`** (`Host: capture composed GDI window surfaces in headless
      frames`) connects the existing display seam to the actual headless PPM
      writer, including 32-bit ARGB conversion and depth reporting. Window
      surfaces track writes; EndPaint/ReleaseDC and message pumps publish dirty
      visible top-level surfaces in creation order, with the topmost group last.
      Child DCs already draw into their owning top-level surface. Clean polls
      do not count as new frames. The compositor owns its pixels and preserves
      the DirectDraw primary as its base when present, including a primary
      presentation following window drawing; guest primary memory is unchanged.
      This follows the latest decision's primary-under-window ordering.

      The new **nogame headless_tests** suite compiles the production headless
      presenter with its guest-boot entry renamed. It drives real window/GDI
      imports and checks the PPM pixels, retained-DC pump presentation, clean
      polls, hidden surfaces, topmost composition, and the DirectDraw base.
      The initial five capture assertions failed; the final suite passes
      **49 checks, 0 failures**. No fake frame counter or launcher input is added.

  31. **Fixed a resize notification feedback loop before painting.** The first
      run after finding 30 (`build/task13-run-30.log`) repeatedly dispatches
      messages through the geometry handler at `00966434`, reaches
      SetWindowPos from `00966229`, and returns to the pump without BeginPaint.
      The shim posted WM_MOVE/WM_SIZE whenever their suppression flags were
      absent, including requests for the unchanged position and size. A layout
      handler setting the same geometry could therefore keep paint behind an
      endlessly replenished message queue.

      Kit **`2e98a29`** (`Runtime: notify window geometry only when it changes`)
      compares the requested geometry before notifying or invalidating it.
      Its unchanged-geometry regression failed (and left a second queued
      notification that failed the following assertion); afterward the runtime
      suite passes **888 checks, 0 failures, 1 skipped** and headless capture
      still passes **49 checks**. Verbose SetWindowPos diagnostics now include
      the geometry, flags, and whether position or size actually changed.


      Run 30 finishes at its 180-second wall-clock cap with **zero frames**
      (line **39947034**), then guest ExitProcess(0), host exit **1**. After
      the geometry fix, run 31 shows unchanged SetWindowPos requests as
      `changed=0/0` (for example **676230**, **676504**) and reaches BeginPaint
      at **676562**, EndPaint at **676816**, and the first capture at **676817**.
      The 320x406 form is positioned at **352,181** in the 1024x768 composite.

      The first captured file, `build/task13-frames-31/frame_0000.ppm`, is
      **1024x768**, **130 exact RGB colours** (the host's coarse colour buckets
      report 29), and **129,920 non-background pixels**. Inspection of the
      converted PNG shows a **magenta 320x406 rectangle on a black background,
      with one small beige checkmark-like patch near its lower right**. There
      are no readable labels or normal form controls in the capture. This is
      evidence that painting and capture execute, not a fully rendered menu.
      The guest enters WaitMessage after the paint. No launcher input is sent.

      **Final run 31 outcome:** the count stays at **one presented frame**.
      At line **40319726**, the host posts WM_CLOSE at **180.0 seconds / one
      frame**. Lines **40320593–40320594** record ExitProcess(0); the summary
      at **40320600–40320601** confirms one written 32-bit frame, and line
      **40320617** confirms guest exit code zero. The process also exits **0**.
      Import statistics record **3,603,945 PeekMessageW calls**, **3,603,898
      WaitMessage calls**, and exactly **one BeginPaint / EndPaint pair**.
      There is no RaiseException, SEH abort, or unknown-target diagnostic.
      Optional lookup misses remain GetLogicalProcessorInformation,
      RtlCompareUnicodeString, InitializeConditionVariable, and DirectXFileCreate;
      missing modules remain msctf.dll, d3dxof.dll, and uxtheme.dll. Startup
      proceeds past each without an exception.

  Validation for findings 30–31, from the game root (all native builds through
  the existing test/build-tool helper; logs and frames remain ignored):

  ```sh
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task13-f30a-runtime-red.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task13-f30a-runtime-green.log 2>&1
  .venv/bin/python build/task-k1-native.py headless_tests --verbose > build/task13-f30b-headless-red.log 2>&1
  .venv/bin/python build/task-k1-native.py headless_tests --verbose > build/task13-f30b-headless-green.log 2>&1
  .venv/bin/python build/task-k1-native.py gdi_tests --verbose > build/task13-f30b-gdi.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task13-f30b-runtime_tests.log 2>&1
  .venv/bin/python build/task-k1-native.py dx_tests --verbose > build/task13-f30b-dx_tests.log 2>&1
  .venv/bin/python build/task-k1-native.py host_tests --verbose > build/task13-f30b-host_tests.log 2>&1
  .venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py > build/task13-f30-config.log 2>&1
  .venv/bin/python tools/build.py --target headless --jobs 8 > build/task13-f30-headless-build.log 2>&1
  RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1 RECOMP_PROFILE_DIR=build/task13-profile-30 RECOMP_FRAMES=build/task13-frames-30 RECOMP_FRAME_EVERY=1 build/recomp/pop_headless > build/task13-run-30.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task13-f31-runtime-red.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task13-f31-runtime-green.log 2>&1
  .venv/bin/python build/task-k1-native.py headless_tests --verbose > build/task13-f31-headless-tests.log 2>&1
  .venv/bin/python tools/build.py --target headless --jobs 8 > build/task13-f31-headless-build.log 2>&1
  RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1 RECOMP_PROFILE_DIR=build/task13-profile-31 RECOMP_FRAMES=build/task13-frames-31 RECOMP_FRAME_EVERY=1 build/recomp/pop_headless > build/task13-run-31.log 2>&1
  .venv/bin/python kit/tools/recomp/ppm_to_png.py build/task13-frames-31/frame_0000.ppm build/task13-frames-31/frame_0000.png
  ```

  - Paint runtime regression: **887 checks, 5 failures, 1 skipped**, exit 1;
    final green **887 checks, 0 failures, 1 skipped**, exit 0. During the
    first implementation run, two old geometry tests also required narrowing
    their filters to the WM_SIZE messages they actually assert.
  - Headless capture red: **37 checks, 5 failures**, exit 1. Final capture
    coverage including DirectDraw: **49 checks, 0 failures**, exit 0, both
    before and after the geometry fix. The test setup was adapted to manual
    guest-stack writes (there is no push32 helper) and runtime defaults
    instead of linking mod hooks that require generated game tables.
  - Existing GDI: **73 checks, 0 failures**; runtime: **887 checks, 0 failures,
    1 skipped**; DirectX: **138819 checks, 0 failures**; host: **3958362 checks,
    0 failures**. Every native suite reports **100% tests passed** and exits 0.
    Game config/literal pytest: **8 passed**, exit 0. The runtime skip remains
    the existing imported-data-symbol check for an image without such symbols.
  - Geometry regression red: **888 checks, 2 failures, 1 skipped**, exit 1;
    green: **888 checks, 0 failures, 1 skipped**, exit 0.
  - Both headless rebuilds link successfully, exit 0; the existing common
    section alignment warning remains. **No regeneration** was needed.
    Formatting and repository/literal/whitespace checks pass before each kit
    commit. Formatting processes 269 sources before the paint commit and
    270 after adding the headless test. The helper now also selects the new
    `headless_tests` CTest target; the root wrapper still has no `-R` option.

  **Task 13's revised acceptance remains unmet:** run 31 exits 0 only after
  the wall-clock cap and presents **1/600 frames**, with the incomplete image
  described above as its first and last capture. This continuation closes
  the approved paint/presenter gaps and records the first live milestone,
  but does not establish a fully rendered startup form, 600 frames before
  the cap, or gameplay. Stop here under the explicit instruction to report
  a painted form whose frame count stalls. No artificial repeat presents,
  forced invalidation loop, automatic click, or Task 14 smoke work is added.


- **2026-09-15 — Task 13 complete: display-clock refresh and the 600-frame
  acceptance (findings 32–33).** The orchestrator identifies run 31 above as
  the **first live paint** of the actual 320x406 startup settings form. Its
  checkmark button is near (578,493) in the 1024x768 capture. A static window
  correctly receives no further WM_PAINT after validation; the one-frame
  stall was the host presentation policy, not a reason to invalidate or
  dismiss the form. The prior incomplete outcome remains recorded above.

  32. **Static GDI refresh on the display clock.** Run 31 line **40320600**
      reports one frame despite millions of message polls. Kit **`8a6c27c`**
      (`Host: refresh static GDI windows on the display clock`) makes the
      headless and smoke host ticks refresh retained visible window surfaces
      at the virtual display's **60 Hz** rate. Each present advances the
      existing pinned guest clock exactly once; RECOMP_PIN_CLOCK's configured
      start and step remain unchanged. Missed display ticks are coalesced,
      rather than emitting a burst of stale frames after a long guest call.
      Recent DirectDraw primary presents own the cadence. After two refresh
      intervals without one, window refreshes resume with the idle primary
      as their base. Primary memory is not changed by composition.

      The smoke host now measures and captures 32-bit GDI snapshots through
      the same path as its primary frames and seals them for its presenter.
      Its primary frames also composite visible window surfaces. Both hosts
      honor RECOMP_FRAMES and RECOMP_FRAME_EVERY; existing smoke scripted dumps
      remain independent. No WM_PAINT loop or input injection was added.

      The painted-once regression failed **8 assertions** before the change:
      **58 checks, 8 failures**. Afterward, including primary-cadence takeover,
      resumption over an idle primary, and the absence of frames without
      visible surfaces, **headless_tests passes 63 checks**. Four refresh
      ticks produce four more captures of unchanged pixels and advance a
      pinned clock from 100 to 300 with a 50 ms step. The initial test setup
      used an assumed sleep helper; it was corrected to the actual
      os_sleep_us API before observing the behavioral failures.

  33. **Import return values for the drawing handoff.** Run 31 logs the drawing
      calls (for example BitBlt at **676814**) but not their results. Kit
      **`a764102`** (`Runtime: include return values in verbose import traces`)
      adds `<- module!function (eax=...)` under the existing RECOMP_LOG=2 switch.
      A subprocess regression checks both a nonzero value and FALSE. It fails
      **2 assertions** before the implementation, then passes with the full
      runtime suite: **893 checks, 0 failures, 1 skipped**. The isolated
      fixture preserves the requested uppercase DLL spelling, unlike this
      image's lowercase import descriptions; the assertions were corrected
      and the failing/passing sequence repeated. No drawing semantics changed.

  **Run 32 is the acceptance run.** With the command below, the process exits
  **0**, reaches **600 presented frames in 14.3 seconds**, and posts WM_CLOSE
  for the frame cap at `build/task13-run-32.log` line **3749581**. The guest
  calls ExitProcess(0) at **3751315**; the summary at **3751320–3751338** confirms
  elapsed time, **600 frames / 10 written captures**, and guest exit code zero.
  The wall-clock cap is not reached. Import statistics record **108,954
  PeekMessageW calls**, **108,907 WaitMessage calls**, and one BeginPaint /
  EndPaint pair. The first paint/capture is at **1353033–1353542**.

  All ten sampled PPMs (frame_0000 through frame_0540, every 60th present)
  have the same SHA-256 as one another: they are refreshes of the static
  startup form. The last sampled image was converted to
  `build/task13-frames-32/frame_0540.png` and visually inspected. It retains
  the magenta background and visible checkmark glyph, with no readable labels.
  This establishes the approved startup-form/message-loop acceptance, not
  the main menu, progression past the form, or gameplay. No click was sent.

  No RaiseException, SEH failure, unallocated-trampoline warning, unknown
  target, or abort occurs. The existing optional misses remain
  GetLogicalProcessorInformation, RtlCompareUnicodeString,
  InitializeConditionVariable, and DirectXFileCreate, plus msctf.dll,
  d3dxof.dll, and uxtheme.dll. Startup proceeds past each without an exception;
  no stub was invented for them.

  **Task 14 run report — initial observations, recorded without fixes:**
  the background bitmap does not draw; magenta is the form color or a
  transparency key showing through. No label text is visible. The checkmark
  button's glyph does draw. Run 32's traced drawing results are:

  | API | Calls | Returned EAX | Evidence in run 32 |
  | --- | ---: | --- | --- |
  | LoadBitmapW | 0 | Not called | No call trace; listed as not reached |
  | LoadImageW | 0 | Not called | No call trace |
  | CreateDIBitmap | 0 | Not called | No call trace; listed as not reached |
  | SetDIBitsToDevice | 0 | Not called | No call trace; listed as not reached |
  | StretchDIBits | 1 | 0x00000018 (24) | Return line 1353410; caller continuation 00bce888 |
  | BitBlt | 5 | 0x00000001 on all five | Return lines 1350147, 1350149, 1353224, 1353242, 1353538 |
  | ExtTextOutW | 0 | Not called | No call trace; listed as not reached |
  | DrawTextW | 3 | 0x00000010 (16) on all three | Return lines 2513, 4097, 1353520; caller continuation 00974978 |

  **None of the called APIs in this requested set reports failure.** Their
  returned success values do not prove that the expected bitmap or label
  pixels reached the form; resolving that discrepancy belongs to Task 14.

  Validation commands, from the game root; all logs, profiles, and captures
  are ignored local artifacts:

  ```sh
  .venv/bin/python build/task-k1-native.py headless_tests --verbose > build/task13-f32-headless-red.log 2>&1
  .venv/bin/python build/task-k1-native.py headless_tests --verbose > build/task13-f32-headless-green.log 2>&1
  .venv/bin/python build/task-k1-native.py host_tests --verbose > build/task13-f32-host-tests.log 2>&1
  .venv/bin/python build/task-k1-native.py gdi_tests --verbose > build/task13-f32-gdi-tests.log 2>&1
  .venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py > build/task13-f32-config.log 2>&1
  .venv/bin/python tools/build.py --target smoke --jobs 8 > build/task13-f32-smoke-build.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task13-f33-runtime-red.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task13-f33-runtime-green.log 2>&1
  .venv/bin/python tools/build.py --target headless --jobs 8 > build/task13-f33-headless-build.log 2>&1
  .venv/bin/python tools/build.py --target smoke --jobs 8 > build/task13-f33-smoke-build.log 2>&1
  RECOMP_MAX_FRAMES=600 RECOMP_LOG=2 RECOMP_IMPORT_STATS=1 RECOMP_PROFILE_DIR=build/task13-profile-32 RECOMP_FRAMES=build/task13-frames-32 RECOMP_FRAME_EVERY=60 build/recomp/pop_headless > build/task13-run-32.log 2>&1
  .venv/bin/python kit/tools/recomp/ppm_to_png.py build/task13-frames-32/frame_0540.ppm build/task13-frames-32/frame_0540.png
  ```

  - Headless red: **58 checks, 8 failures**, exit 1; final green **63 checks,
    0 failures**, exit 0. Host: **4,050,705 checks, 0 failures**, exit 0.
    GDI: **73 checks, 0 failures**, exit 0. Config/literal pytest: **8 passed**,
    exit 0. Native green runs report **100% tests passed**.
  - Return-trace runtime red: **893 checks, 2 failures, 1 skipped**, exit 1;
    green: **893 checks, 0 failures, 1 skipped**, exit 0. The skip remains
    the image's lack of imported data symbols.
  - Both hosts build successfully, exit 0. Smoke retains existing C-linkage
    warnings for user-defined return types; both links retain the existing
    common-section alignment warning. Smoke was built, not used to drive
    the guest or perform Task 14 actions.
  - No translator change or regeneration. The actual switches remain
    RECOMP_MAX_FRAMES and RECOMP_LOG. Native suites use the existing ignored
    helper because the root test wrapper does not accept -R; it delegates
    builds to the kit tools and selects CTest targets.
  - Before each kit commit, `.venv/bin/python kit/tools/format.py --write`
    formatted **270 files**, and repository-boundary, game-literal, and
    staged whitespace checks passed. Their outputs are in the corresponding
    `build/task13-f32-*` and `build/task13-f33-*` check logs.

  **Task 13 is complete under the orchestrator's final acceptance.** Run 31
  is the first live paint; run 32 proves the 600-frame, exit-0 startup-form
  run before the wall-clock cap. The drawing defects above are handed to
  Task 14, with their observed call results and no speculative rendering fix.

- **2026-09-15 — Task 14: DrawText paints; main-menu acceptance blocked by
  smoke input routing.** Kit **`35277ef`** (`GDI: rasterize wide DrawText
  through the bitmap canvas`) fixes the observed successful-but-invisible
  DrawTextW calls. The USER32 entry previously returned metrics and logged
  that rasterization was unavailable; it never reached the bitmap font.
  It now delegates to GDI, sharing ExtTextOutW's glyph renderer, selected
  font, text/background colours and canvas clipping. Basic alignment,
  CRLF, word wrapping, tab expansion and mnemonic processing are supported.
  CALCRECT measures without painting, including a screen DC with no backing
  surface. DrawTextExW uses the same path. This follows the
  [DrawTextW contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-drawtextw).
  Full font-face fidelity and advanced layout remain outside this fix.

  **Test-first evidence:** the new pixel/metrics regression failed **4
  assertions, 82 checks** before implementation. The first green run passed
  all 82; added layout coverage passed 85. Reviewing the diagnostic run
  exposed a new CALCRECT regression on screen DCs: a dedicated test failed
  **1 assertion, 86 checks**, then passed after moving the backing-surface
  requirement to the drawing path. Final GDI: **86 checks, 0 failures**.
  Runtime: **893 checks, 0 failures, 1 skipped** (the existing imported-data
  prerequisite skip). Config/literal pytest: **8 passed**.

  **Paint findings:** the first smoke capture reproduces the 320x406 form
  at (352,181), with its 24x24 checkmark at (567,482) through (590,505).
  Despite `RECOMP_SMOKE_DRAWABLE=800x600`, the retained GDI guest surface is
  **1024x768**: `gdi_present_windows` uses that desktop size before a
  DirectDraw primary exists. The script's intended click is (578,493).

  The pinned executable's `TFRMLAUNCHSETTING` resource explicitly declares
  `Color=clFuchsia`, `TransparentColor=True` and
  `TransparentColorValue=clFuchsia`. Its `imgBack` is a transparent,
  client-aligned TImage with **no Picture.Data**; `imgCheck` carries a 24x24
  PNG. This confirms that the magenta comes from the form's own colour/key,
  not COLOR_BTNFACE (the kit's value for index 15 remains `0x00f0f0f0`).
  A temporary brush trace records a solid brush with `color=00ff00ff`
  immediately before the form paint's FillRect. The actual drawn version
  label uses `text_color=0015585f`, transparent background mode and flags
  `0x40`; it is neither magenta nor equal to its background. Its pixels now
  appear as dark olive **1.03.1** at (378,205) through (420,218). Other
  control labels and the skin are still absent. No background-image file
  open was observed: the only CreateFileW call opens Siege.log. The path
  intended to populate imgBack is still unverified.

  **Blocking input evidence:** run 3's temporary diagnostic trace records:

  ```text
  TASK14 INPUT hwnd=00020004 msg=200 xy=578,479 visible=0 class=#atom49152
  TASK14 INPUT hwnd=00020004 msg=201 xy=578,479 visible=0 class=#atom49152
  TASK14 INPUT hwnd=00020004 msg=202 xy=578,479 visible=0 class=#atom49152
  ```

  `build/task14-run-3.log` lines 1780009–1780010 and 1805019 identify the
  mouse move/down/up. Creation logs identify **0x00020004** as the hidden
  `tputilwindow`, and **0x00020010** as the startup form. The smoke host's
  `move_by` clamps absolute coordinates to **639,479**; its `post` always
  sends to `host_main_window()`. USER32 retains the first-created window
  as that handle. Thus the requested click is both clamped and delivered
  to the wrong window. Script-coordinate adjustment cannot correct the
  recipient. Fixing generic host pointer bounds, visible-window targeting,
  client-coordinate conversion and capture routing requires host/USER32
  input work outside Task 14's stated drawing files; no such fix or guest
  callback bypass was added. All temporary diagnostics were removed before
  the kit commit and the final smoke build.

  **Final clean smoke run:** `build/run-smoke.log`, exit **0**, reports
  **12.0 seconds**, **462 presented frames**, script **3/3 actions** and
  guest exit code **0**. Both dumps still show the startup settings form.
  They have the magenta rectangle, checkmark and newly drawn version text;
  there is no title screen or menu. `dump` actually names its output
  `smoke_main-menu_present.ppm`, so it was copied to the planned
  `main-menu.ppm` name before conversion. The acceptance measurement is
  **`(1024, 768) 131`**, failing both the requested dimensions and the
  greater-than-1000-colours condition. The script has no image assertions;
  its `all expectations met` footer is not main-menu acceptance.

  The final log contains **5 BitBlt and 5 StretchBlt entries** (one of the
  latter is the StretchDIBits adapter's internal call), **0 DirectDraw
  calls**, and therefore **0 DirectDraw Blt/Flip calls**. DrawTextW is
  called three times and returns 16 each time. StretchDIBits returns 24;
  LoadBitmapW, LoadImageW, CreateDIBitmap, SetDIBitsToDevice and ExtTextOutW
  remain uncalled. There is no unknown-target or unhandled-exception
  diagnostic. DirectDraw mode selection, fallback interfaces and primary
  composition cannot be assessed until input dismisses the form. No INI
  override was needed or applied; no DirectDraw mode request was reached.

  Exact validation commands, from the game root (outputs remain ignored):

  ```sh
  .venv/bin/python build/task-k1-native.py gdi_tests --verbose > build/task14-text-red.log 2>&1
  .venv/bin/python build/task-k1-native.py gdi_tests --verbose > build/task14-text-green.log 2>&1
  .venv/bin/python build/task-k1-native.py gdi_tests --verbose > build/task14-measure-red.log 2>&1
  .venv/bin/python kit/tools/format.py --write > build/task14-format.log 2>&1
  .venv/bin/python build/task-k1-native.py gdi_tests --verbose > build/task14-text-final.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task14-runtime.log 2>&1
  .venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py > build/task14-config.log 2>&1
  .venv/bin/python kit/tools/check_game_literals.py > build/task14-literals.log 2>&1
  .venv/bin/python kit/tools/check_repo.py > build/task14-repo-check.log 2>&1
  git -C kit diff --cached --check
  .venv/bin/python tools/build.py --target smoke --jobs 8 > build/build-smoke.log 2>&1
  RECOMP_LOG=2 RECOMP_IMPORT_STATS=1 RECOMP_PROFILE_DIR=build/task14-profile-4 RECOMP_SCRIPT=$PWD/smoke/main-menu.script RECOMP_HOST_DUMP_DIR=$PWD/build/smoke RECOMP_DDRAW_MODES=800x600x16,800x600x32 RECOMP_SMOKE_DRAWABLE=800x600 build/recomp/pop_smoke > build/run-smoke.log 2>&1
  cp build/smoke/smoke_main-menu_present.ppm build/smoke/main-menu.ppm
  .venv/bin/python kit/tools/recomp/ppm_to_png.py build/smoke/main-menu.ppm build/smoke/main-menu.png
  .venv/bin/python -c "from PIL import Image; im = Image.open('build/smoke/main-menu.png'); print(im.size, len(set(im.getdata())))"
  ```

  - Red commands exit 1 with `82 checks, 4 failures` and
    `86 checks, 1 failures`; final native commands exit 0 with
    `100% tests passed, 0 tests failed out of 1` and the counts above.
  - Formatting: `Formatted 270 handwritten source files`; repository check:
    `Tracked source boundaries and local documentation links passed`.
    Literal and staged whitespace checks exit 0 without output.
  - Build exits 0; the existing common-section alignment linker warning
    remains. Converter prints `(1024x768)`; the pixel check prints
    `(1024, 768) 131` and exits 0, although acceptance fails.
  - As in Task 13, the root test wrapper has no `-R`; the existing ignored
    helper delegates native builds to the kit's test/build modules, then
    selects CTest. No direct compiler invocation. The text adapter required
    a delegation change in `runtime/user32_wide.cpp` in addition to GDI.
  - Only macOS native validation ran. No translator change, regeneration,
    push, player-save change, private input commit or other plan task.

  **Task 14 is incomplete.** The smoke script and tested text fix are
  committed with this blocked handoff; the changelog does not claim that
  the main menu draws. Resume after the input-routing scope is resolved.


- **2026-09-15 — Task 14 resumed: input reaches Play; main-menu acceptance
  remains blocked before DirectDraw.** The orchestrator explicitly brought
  shared screen coordinates and Win32 mouse targeting/capture into scope.
  Five tested kit fixes are committed on `siege-delphi`:

  | Kit commit | Change | Red -> green evidence |
  | --- | --- | --- |
  | `0aaf989` | Shared virtual screen for metrics, GDI dumps and smoke pointer bounds; a selected DirectDraw mode takes precedence | GDI 88 checks / 2 failures -> 88 / 0; DX 138822 / 0 |
  | `0ed2189` | Mouse hit testing in visible/enabled stacking order, children, client coordinates, activation and capture; Shift/Control MK flags | Runtime 905 / 12 -> 906 / 0, with a separate modifier regression 906 / 1 -> 906 / 0; headless 63 / 0 |
  | `0605731` | Synchronous SetWindowPos notifications so VCL sees new dimensions before setting the next one | Runtime 911 / 5 -> 911 / 0; headless 63 / 0 |
  | `17ee0aa` | RET into a resolved import executes that import, preserving the first Delphi delay-load call | SEH 117 / 4 -> 120 / 0 (the additional checks run inside the formerly skipped target) |
  | `93b80a3` | Incremental builds refresh the adjacent runtime header without regenerating the translated sources | New build test 1 failed -> combined build suites 31 passed |

  Runtime suites retain the existing **1 skipped** imported-data prerequisite.
  The input regression covers overlapping windows, topmost order, hidden and
  disabled windows, child coordinates, capture outside the window, release,
  activation/eaten presses, and keyboard modifier flags. GDI presentation
  uses the same stacking order as input. Before DirectDraw selects a mode,
  `RECOMP_SMOKE_DRAWABLE=800x600` now gives both a dump and click space of
  800x600, with the smoke host asserting agreement. The default remains
  1024x768. No DirectDraw implementation change was needed or tested by a
  live game call; its unit test verifies that a selected mode takes priority.

  **Correction to the earlier handoff: the checkmark is the fullscreen
  checkbox, not confirmation.** Run 6 delivers move/down/up to the startup
  form `00020010` at client `(226,312)` and toggles that checkbox. The form's
  `imgBackClick` listing (`functions/00bece70.asm`) tests the fullscreen
  rectangle `(214,300)-(240,326)` separately from the Play rectangle
  `(400,320)-(520,385)`, which calls `Done` at `00bebdec`. The initial
  320-pixel form width placed Play outside the visible client area.

  The form first calls SetWindowPos with 552x240, then 320x406. The old
  queued WM_SIZE left VCL's cached width stale between those calls. Sending
  WM_WINDOWPOSCHANGED synchronously, with WM_MOVE/WM_SIZE derived by
  DefWindowProc, preserves **552x406**, centered at **(124,97)** in the
  800x600 virtual screen. This follows the
  [Windows position/size notification contract](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features).
  The script now clicks **Play at (584,450), client (460,353)**, after its
  startup dump. It retains the later `dump main-menu` action for resumption.

  **The skin decoder is identified.** Neither `windowscodecs.dll` nor
  `gdiplus.dll` is loaded near form creation (or anywhere in these traces).
  `FormCreate` at `00bec200` constructs the guest `TPngImage` class and
  calls `00bcef24` to read the `STARTUPBACK` RCDATA resource. The pinned PE
  contains a **552x406 RGBA PNG, 352355 bytes** there. FindResourceW,
  LoadResource, SizeofResource and LockResource succeed. It is decoded by
  Delphi's own PNG code, not a missing WIC/GDI+ module.

  FormCreate calls AlphaBlend at `00bec36d`, through thunk `008168b0`.
  The delay adapter at `00816890` calls the resolver, restores ECX/EDX,
  exchanges saved EAX with the resolved target on the stack, then RETs to
  that target. GetProcAddress returned `0ff04870`, but the old RET dispatcher
  only followed generated code entries, so the first AlphaBlend never ran.
  The runtime fix and refreshed header now produce an observed
  **AlphaBlend return of 1**. The first build had retained the stale
  `gen/x86.h` even though native runtime tests used the new canonical header;
  the build regression reproduces this and ensures subsequent unchanged
  builds preserve the header timestamp and existing translation. No
  translated source was edited or regenerated.

  **Text is verified by pixels.** A temporary DrawText trace (removed before
  the final build) records three calls: two with DT_CALCRECT, and one actual
  draw of `1.03.1`, rectangle `(0,0)-(48,16)`, DC origin `(-24,-24)`, foreground
  COLORREF `0015585f`, transparent background. In run 7, the corresponding
  screen rectangle **(148,121)-(196,137)** contains **122 pixels of RGB
  (95,88,21)** and 646 magenta pixels. It is real glyph output, not just a
  successful return value. The same 122 foreground pixels remain after
  AlphaBlend starts working. The form resource explicitly selects
  clFuchsia as its fill/transparency key, as recorded in the prior entry;
  COLOR_BTNFACE remains `00f0f0f0`.

  **Clean final smoke evidence:** `build/run-smoke.log`, host exit **6**.
  The startup capture shows the title artwork, Monitor/Resolution/Fullscreen/
  Language labels, checkmark, and Play, with a largely magenta interior
  instead of the source PNG's parchment. Settings value text is still
  absent. Its size/count is **(800,600), 13000 distinct colours**. This is
  the startup settings form, not the main menu. The trace records Play's
  move/down/up on `00020010`, the modal teardown, then creation of
  `TfrmMain` (`0002002c`) and an **800x600 TPanel** (`00020030`). The run has
  **5 BitBlt, 7 StretchBlt entries**, one AlphaBlend and **zero DirectDraw
  calls / DirectDraw Blt or Flip calls**. No display mode is requested, so
  no ScreenResolution seed was applied.

  **New blocker, reproduced in run 9 and the clean final run:** the media
  constructor `00bc3414.asm` calls CoInitializeEx at `00bc344d`, then
  `MFStartup(0x20070,0)` through `00bc18e8` at `00bc3459`. LoadLibraryA reports
  `mfplat.dll` missing, GetLastError returns `0000007e`, and the delay-load
  helper raises. During the attempted recovery:

  ```text
  RtlUnwind: registration=0efffabc target_ip=0080a100 retval=00000000
  SEH: unwind target has no live checkpoint (registration=0efffabc target=00809542 FS=0fe00000 ESP=0efff9d8)
  [host] an abort from the runtime in guest thread 1: EIP=0080a12a ESP=0efff9d8 EBP=0efffae0
  ```

  The plan's assumption that the missing Media Foundation module is skipped
  cleanly is **not established**. The immediate blocker is exception recovery
  through the missing constructor checkpoint, before any DirectDraw display.
  No Media Foundation stub, OS-version workaround, direct guest-handler call,
  translator change or movie exclusion was added. Resuming needs a scoped
  SEH/constructor-translation investigation (Tasks 12/13), then another
  smoke run. The remaining startup transparency/value-text defects and the
  DirectDraw/main-menu checks remain open.

  **Acceptance failed:** execution aborts before `dump main-menu`; neither
  a current `build/smoke/main-menu.ppm` nor `main-menu.png` exists. Earlier
  startup-only files with those names were archived under ignored
  `build/task14-previous-smoke/`. The final startup evidence is
  `build/smoke/smoke_startup-form_present.ppm` and `startup-form.png`.
  A 13000-colour startup image does not satisfy the title-screen/menu target.

  Exact test/build commands (from the game root; logs are ignored):

  ```sh
  .venv/bin/python build/task-k1-native.py gdi_tests --verbose > build/task14-coordinates-red.log 2>&1
  .venv/bin/python build/task-k1-native.py gdi_tests --verbose > build/task14-coordinates-green.log 2>&1
  .venv/bin/python build/task-k1-native.py dx_tests --verbose > build/task14-coordinate-dx.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task14-routing-red.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task14-routing-modifiers-red.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task14-routing-final.log 2>&1
  .venv/bin/python build/task-k1-native.py headless_tests --verbose > build/task14-routing-headless.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task14-geometry-red.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task14-geometry-green.log 2>&1
  .venv/bin/python build/task-k1-native.py headless_tests --verbose > build/task14-geometry-headless.log 2>&1
  .venv/bin/python build/task-k1-native.py seh_tests --verbose > build/task14-ret-red.log 2>&1
  .venv/bin/python build/task-k1-native.py seh_tests --verbose > build/task14-ret-green.log 2>&1
  .venv/bin/python -m pytest -q kit/tools/tests/test_build.py -k incremental_build > build/task14-header-red.log 2>&1
  .venv/bin/python -m pytest -q kit/tools/tests/test_build.py kit/tests/test_build_py.py > build/task14-header-green.log 2>&1
  .venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_seh.py > build/task14-ret-translator.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task14-runtime-final.log 2>&1
  .venv/bin/python build/task-k1-native.py gdi_tests --verbose > build/task14-gdi-final.log 2>&1
  .venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py > build/task14-config-final.log 2>&1
  .venv/bin/python kit/tools/check_repo.py > build/task14-repo-final.log 2>&1
  .venv/bin/python kit/tools/check_game_literals.py > build/task14-literals-final.log 2>&1
  .venv/bin/python tools/build.py --target smoke --jobs 8 > build/build-smoke.log 2>&1
  RECOMP_LOG=2 RECOMP_IMPORT_STATS=1 RECOMP_PROFILE_DIR=build/task14-profile-final RECOMP_SCRIPT=$PWD/smoke/main-menu.script RECOMP_HOST_DUMP_DIR=$PWD/build/smoke RECOMP_DDRAW_MODES=800x600x16,800x600x32 RECOMP_SMOKE_DRAWABLE=800x600 build/recomp/pop_smoke > build/run-smoke.log 2>&1
  .venv/bin/python kit/tools/recomp/ppm_to_png.py build/smoke/smoke_startup-form_present.ppm build/smoke/startup-form.png
  .venv/bin/python -c "from PIL import Image; im = Image.open('build/smoke/startup-form.png'); print(im.size, len(set(im.getdata())))"
  ```

  Red commands exited 1 with the counts above. Final runtime: **911 checks,
  0 failures, 1 skipped**; final GDI: **88 checks, 0 failures**. All green
  native selections end with `100% tests passed, 0 tests failed out of 1`.
  Build suites: **31 passed**; translator SEH suite: **10 passed**;
  config/literal pytest: **8 passed**. Smoke build exits 0 with the existing
  common-section alignment linker warning. Converter reports `(800x600)`;
  startup pixel check prints `(800, 600) 13000`. Repository boundary check
  reports `Tracked source boundaries and local documentation links passed`;
  literal/whitespace checks exit 0 without output.

  Before each kit commit, `.venv/bin/python kit/tools/format.py --write`
  reports `Formatted 270 handwritten source files`, followed by the
  repository/literal/staged-whitespace checks. Their logs use the prefixes
  `task14-coordinate`, `task14-routing`, `task14-geometry`, `task14-ret` and
  `task14-header`. As previously recorded, `tools/test.py` lacks `-R`; the
  ignored helper delegates configure/build to the kit test/build modules
  and selects CTest. Native compilation never invokes a compiler directly.
  Validation is macOS only; no push, regenerated translation, private input
  commit, hook/sentinel change, player-save edit or other platform task.

  **Task 14 remains incomplete.** This commit records the tested input and
  startup-drawing progress with the exact pre-DirectDraw blocker. It does
  not claim that the main menu draws.

- **2026-09-15 — Task 14 resumed: constructor checkpoints and unsupported
  Media Foundation exports; main menu still blocked by exception return
  control.** Work stayed in the `kit/` submodule on `siege-delphi`. Three
  kit commits are now pinned:

  | Kit commit | Change | Verification |
  | --- | --- | --- |
  | `a65fed1` | Checkpoint enter/leave/unwind traces include their establishing instructions | Inherited red log: 1 failed, 10 passed; fresh SEH translator run: 11 passed; native SEH: 120 checks, 0 failures |
  | `ac8abde` | Recognize the Delphi constructor helper's caller-owned registration; create the checkpoint after the helper returns, retire it on absolute `POP FS:[0]`, and recover its handler landing | New driver regression: 2 failed, 15 passed -> combined SEH/driver suites: 152 passed; native SEH: 120 checks, 0 failures |
  | `2d2eafc` | Present-but-unsupported `mfplat.dll` and `mf.dll` exports | Runtime: 960 checks, 48 failures, 1 skipped -> 944 checks, 0 failures, 1 skipped; missing exports add failure checks in the red run |

  **First decision, established by the regenerated trace:** address
  `0efffabc` had earlier, unrelated registration lifetimes which entered
  and retired normally. The media constructor's registration at that address
  had **never entered**, rather than being retired by a nested callback.
  `build/task14-handler-trace-run.log` records the actual handler read from
  `registration + 4`:

  ```text
  SEH handler: registration=0efffabc handler=0080953d flags=00000000
  SEH: unwind target has no live checkpoint (registration=0efffabc target=00809542 FS=0fe00000 ESP=0efff9d8)
  ```

  `functions/00809514.asm` stores handler `0080953d` at `0080952c`, then
  establishes `FS:[EDX] = ECX` at `00809536`. ECX addresses the caller's
  reserved 16-byte record. This helper restores its saved registers and
  returns, so putting setjmp inside it would leave a dead host checkpoint.
  The media constructor reserves those bytes at `00bc3423` and calls the
  helper at `00bc3426`, before its ordinary frame at `00bc343d`.

  The translator now recognizes the complete compiler helper sequence,
  with its handler address read from the image, and the preceding 16-byte
  reservation at each call site. It places setjmp in the caller after that
  call. The driver regression relocates the fixture to two synthetic
  addresses, verifies the helper itself has no checkpoint, checks the
  normal `POP FS:[0]` retirement and recovered landing provenance, and
  rejects a short reservation or an unproven FS base. No game address was
  added to kit code. The unrelated proposed dispatch-depth retirement fix
  was not applied: this trace did not implicate it.

  **Second decision:** `MFStartup` takes two arguments and returns
  `MF_E_BAD_STARTUP_VERSION` (`c00d36e3`); `MFShutdown` takes none and returns
  S_OK. The seven requested `mf.dll` factory/service exports return
  E_NOTIMPL (`80004001`) and clear their final 32-bit output pointer. This
  is explicit unavailability, not a media implementation. The argument
  shapes were checked against Microsoft documentation, including
  [MFStartup](https://learn.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mfstartup)
  and [MFGetService](https://learn.microsoft.com/en-us/windows/win32/api/mfidl/nf-mfidl-mfgetservice).
  Runtime tests exercise LoadLibraryW/GetProcAddress, return codes, stack
  cleanup, null outputs and neighboring-word preservation. The one runtime
  skip remains the existing imported-data prerequisite.

  **Fresh game chronology after the second regeneration:**

  1. Play dismisses the startup settings form. The virtual screen is
     **800x600** with the script's environment, as the prior run log states;
     the handoff's 1024x768 description was stale. The existing click
     **(584,450)** remains correct.
  2. `MFStartup` resolves and returns `c00d36e3`. MessageBoxW reports
     `Your computer does not support this Media Foundation API version131184.`
     and answers default button 1. Delphi then raises its exception; the
     message being answered does not by itself prove a working no-video path.
  3. The trace now shows registration `0efffabc` established at `00bc3426`,
     and its unwind successfully reaches landing **`00809542`**. There is
     no missing-checkpoint abort. Constructor cleanup calls MFShutdown and
     CoUninitialize successfully.
  4. Exception return control is then wrong. A temporary diagnostic added
     only around `recomp_seh_land`'s guest call recorded:

     ```text
     SEH landing returned: registration=0efffabc EIP=01000c00 ESP=0efffa6c EAX=00000001
     ```

     EIP is a guest heap address and ESP remains below the constructor's
     registration. Execution nevertheless continues in its caller. It
     attempts MFCreateMediaSession, which returns E_NOTIMPL, and handles a
     second exception through landing `00bc431c`. Calls through invalid
     pointers follow; the first target varies with stale memory contents.
  5. The final, **committed-code** reproduction ends with host exit **5**:

     ```text
     call to unknown target 57726566 (ESP=0efffa64, return=00808e63): returning 0
     call to unknown target 00000000 (ESP=0efffa80, return=00966052): returning 0
     [host] SIGSEGV in guest thread 1: EIP=00000000 ESP=00000008 EBP=00000000
     ```

  **Next blocker, source evidence and scope boundary:** the constructor's
  landing calls recovered helper `0080a42c`, which manually removes
  dispatcher stack words and returns disposition 1 (continue search).
  The translated caller at `0080955b` still proceeds to its own RET at
  `00809560`. Meanwhile `recomp_seh_land` assumes that a returned landing
  has finished the establishing function, retires its checkpoint and
  dispatch records, and returns to that function's host caller. This
  evidence points to missing propagation of a guest return across host
  call frames and resumption of the abandoned exception search. It is
  distinct from detecting a registration or preserving one across a
  callback. Correct support needs a tested dispatcher-resumption contract;
  no speculative fix or game-specific exception bypass was added. Task 14
  stops here under the instruction to report blockers beyond the scoped
  checkpoint/Media Foundation work.

  The temporary landing-return probe was removed, followed by a rebuild
  and the clean reproduction above. Its evidence remains only in ignored
  `build/task14-landing-return-run.log`. A batch LLDB attempt never completed
  launch; its owned processes were stopped, and no diagnosis relies on it.
  The permanent handler trace supplied the required `registration + 4`
  evidence without the debugger.

  **Frame/Step 3 evidence:** the clean log has **32** lines matching
  `BitBlt|StretchBlt` (5 BitBlt, 7 StretchBlt and 4 SetStretchBltMode calls,
  each with an entry and return line), **0** matching `Surface.*::Blt|Flip`, and **0 DirectDraw
  import calls**. There is therefore no observed DirectDraw method failure
  or DirectDrawCreateEx fallback to implement yet. The current dump is
  `build/smoke/smoke_startup-form_present.ppm`, converted to `startup-form.png`:
  **(800,600), 13000 distinct colours**. Visual inspection shows the gold
  title artwork, version text, Monitor/Resolution/Fullscreen/Language
  labels, checkbox and Play over a mostly magenta skin, surrounded by black.
  This is the startup form, not the title-screen main menu.

  **Deferred drawing defects:** guest TPngImage decodes the startup PNG.
  The magenta/parchment transparency defect remains to trace through
  AlphaBlend's premultiplied-alpha path and the 32-bpp
  CreateDIBSection/StretchDIBits storage paths. Settings-value text is still
  absent. Keep both items for work after a menu frame exists; no GDI or
  DirectDraw behavior was changed in this resumption.

  **Acceptance fails:** the run aborts before `dump main-menu`.
  `smoke_main-menu_present.ppm`, `main-menu.ppm` and `main-menu.png` are all
  absent. The planned PIL acceptance command exits 1 with FileNotFoundError,
  recorded in `build/task14-resume-acceptance.log`. The script now documents
  the capture-name normalization needed after a successful future run.
  The changelog deliberately does not say that the main menu draws.

  Exact test commands and log locations, from the game root:

  ```sh
  .venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_seh.py > build/task14-resume-trace-python.log 2>&1
  .venv/bin/python build/task-k1-native.py seh_tests --verbose > build/task14-resume-trace-native.log 2>&1
  .venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_seh.py > build/task14-constructor-red.log 2>&1
  .venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_seh.py kit/tools/recomp/tests/test_translate_driver.py > build/task14-constructor-green.log 2>&1
  .venv/bin/python build/task-k1-native.py seh_tests --verbose > build/task14-constructor-native.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task14-mf-red.log 2>&1
  .venv/bin/python build/task-k1-native.py runtime_tests --verbose > build/task14-mf-green.log 2>&1
  .venv/bin/python build/task-k1-native.py dx_tests --verbose > build/task14-resume-dx.log 2>&1
  .venv/bin/python build/task-k1-native.py gdi_tests --verbose > build/task14-resume-gdi.log 2>&1
  .venv/bin/python -m pytest -q kit/tools/recomp/tests/test_translate_insns.py > build/task14-resume-insns.log 2>&1
  .venv/bin/python -m pytest -q tests kit/tests/test_game_literals.py > build/task14-resume-config.log 2>&1
  ```

  Native green tails are `100% tests passed, 0 tests failed out of 1`:
  SEH **120 checks, 0 failures**; runtime **944 checks, 0 failures, 1 skipped**;
  DX **138822 checks, 0 failures**; GDI **88 checks, 0 failures**.
  Instruction suite: **54 passed**; config/literal pytest: **8 passed**.
  Translator red exits 1 with **2 failed, 15 passed**; its combined green
  tail is **152 passed**. Runtime red exits 1 through the helper after CTest
  fails with the counts in the table.

  Build/run/image commands:

  ```sh
  .venv/bin/python tools/build.py --regenerate --target smoke --jobs 8 > build/task14-resume-trace-build.log 2>&1
  .venv/bin/python tools/build.py --regenerate --target smoke --jobs 8 > build/task14-constructor-build.log 2>&1
  .venv/bin/python tools/build.py --target smoke --jobs 8 > build/build-smoke.log 2>&1
  RECOMP_LOG=2 RECOMP_IMPORT_STATS=1 RECOMP_PROFILE_DIR=$PWD/build/task14-resume-final-profile RECOMP_SCRIPT=$PWD/smoke/main-menu.script RECOMP_HOST_DUMP_DIR=$PWD/build/smoke RECOMP_DDRAW_MODES=800x600x16,800x600x32 RECOMP_SMOKE_DRAWABLE=800x600 build/recomp/pop_smoke > build/run-smoke.log 2>&1
  .venv/bin/python kit/tools/recomp/ppm_to_png.py build/smoke/smoke_startup-form_present.ppm build/smoke/startup-form.png > build/task14-resume-startup-image.log 2>&1
  .venv/bin/python -c "from PIL import Image; im = Image.open('build/smoke/main-menu.png'); print(im.size, len(set(im.getdata())))" > build/task14-resume-acceptance.log 2>&1
  ```

  All three builds exit 0; the existing common-section alignment warning
  remains, and the regenerated build also reports existing C-linkage warnings
  in host headers. Smoke exits 5 and the acceptance command exits 1.
  The initial trace-only smoke exits 6 and is retained in
  `build/task14-resume-trace-run.log`; the subsequent handler-read diagnostic
  also exits 6 in `build/task14-handler-trace-run.log`.

  Before each kit commit, the following commands passed. Logs use the
  prefixes `task14-resume-trace`, `task14-constructor` and `task14-mf`, with
  `-format.log`, `-literals.log` and `-repo.log` respectively:

  ```sh
  .venv/bin/python kit/tools/format.py --write
  .venv/bin/python kit/tools/check_game_literals.py
  .venv/bin/python kit/tools/check_repo.py
  git -C kit diff --cached --check
  ```

  Formatting reported 270 sources for the first two commits and 271 after
  adding the Media Foundation module. The source-boundary check reports
  `Tracked source boundaries and local documentation links passed`;
  literal/whitespace checks exit 0 silently. Native suites use the existing
  ignored `build/task-k1-native.py`, because `tools/test.py` has no `-R`;
  it calls the kit's configure/build/test functions, not compilers directly.
  Validation remains macOS only. There was no push, executable-hash bypass,
  guest-address/config change, player-save edit, or private-input commit.
