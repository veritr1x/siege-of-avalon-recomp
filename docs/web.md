# Browser build and optional streaming

Use this repository's pinned `kit/`, your own hash-matched installation, and
the preparation/analysis steps in [CONTRIBUTING.md](../CONTRIBUTING.md).
Install the kit's hashed Python requirements and activate Emscripten 6.0.11.
On Windows, use `.venv/Scripts/python.exe` instead of `.venv/bin/python`.

```sh
.venv/bin/python tools/build.py --target web --regenerate --jobs 4
.venv/bin/python kit/tools/web_launcher.py --game-dir "$PWD" --out build/web-stream-site \
    --web-build siege=build/web/recomp --stream siege --serve 2028
```

After the first translation, omit `--regenerate` when translator inputs have
not changed. Open `http://127.0.0.1:2028/` and choose **Stream game**.
The command stages a private installation and manifest into the site. It
preserves executable/module hash checks, bundle exclusions and prepared
auxiliary modules. Original installation files are unchanged.

If analysis uses a minimal executable copy, select the full installation
explicitly: replace `--stream siege` with
`--stream "siege=/path/to/full installation"`. That directory must contain
the configured executable with its pinned hash and required data folders.
This changes the staging source only, preserving configured analysis inputs.
Keep generated game data and all local evidence private.

The worker mounts game files read-only at `/data`, using 4 MiB HTTP ranges
and a 64 MiB download cache. This is a cache bound, not total browser memory.
The kit supplies a corrected fetch backend without modifying the installed
SDK. Saves and settings use a separate writable OPFS profile.

**Import folder**, ZIP import and **Play** remain available. Play uses an
imported installation; Stream game selects hosting explicitly. Direct player
URLs prefer imported data when available. Both modes share saves on the same
origin. For games with prepared auxiliary modules, import a prepared bundle
with the pinned modules at their configured names; the staged `data/` folder
is such a bundle. A raw install may keep those modules elsewhere.

To prepare an import-only site, use a separate `--out` directory and omit
`--stream`. Previously staged assets are not removed by omitting the flag.
The server supplies HEAD, byte ranges, Wasm MIME and COOP/COEP. Other hosting
must preserve those semantics and same-origin asset URLs; files larger than
64 MiB require ranges. Streaming has no persistent offline asset cache.

Keep the hostname and port consistent to reuse saves. Export saves from
**Manage** before clearing site storage. Browser compatibility results are
recorded below; compilation alone does not establish gameplay.

## Local validation

Updated to recomp-kit `9dff77f71928ce55933c6354ee763a8077d17909`. The real
Emscripten 6.0.11 game build passes without `--allow-unmodelled`. Shared
worker-safe GPU readback and preserved transfer regions resolve the black
output after Play. The browser GPU fixture passes 1,004 checks, including
padded rows, depth checkpoints, command completion and device loss.

A Windows/Chromium 145 hardware-WebGPU streamed run mounts 11,538 files,
verifies the pinned 1.20 executable and supporting module, and reaches the
first castle map with movies disabled. Desktop mouse input completes the
opening dialogue, moves the player, opens/closes the chest inventory, and
navigates the original save/load menus. A named save creates its SAV, MAP,
IDX and screenshot files in the writable OPFS profile; loading it returns
to the rendered world in the same session. The named save also survives a
player-page reload and loads in the final rebuilt player. No JavaScript
exception, failed asset request, readback failure or WebGPU validation error occurs.

This also exposed a second browser blocker: offscreen DirectDraw blits
accumulated in recorder frame 1 while D3D11 presented complete window images.
The kit now retires consumed blit records at that boundary while preserving
pending Direct3D draw snapshots. Wasm memory stays at 2,253,193,216 bytes
through the first map, dialogue, chest, movement and save/load, instead of
climbing to the 4 GB limit and aborting. The DirectX suite passes 142,104
checks and the host suite passes 3,908,091 checks, including retirement,
source-lease release, arena reuse and pending-draw preservation.

Touch-only menu activation was unreliable in this check; desktop mouse input
was used for the completed flow. These are bounded first-map checks. Full
campaigns, audible output, other browsers, folder import for Siege and save durability
across browser restarts are not established. The observed castle frame rate
varies roughly 25-50 FPS; this is not a performance benchmark. The game still
uses substantial browser memory. Original game files and player profiles
are preserved; only disposable browser profiles were used.

All ten portable project tests pass with the MinGW runtime on PATH. Private
build logs, screenshots and browser diagnostics remain ignored. Historical
native/device results were not all rerun for this browser update.
