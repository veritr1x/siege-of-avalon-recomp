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

Updated to recomp-kit `1cbd81ca8e6e1ef2cf33f1c27a06f69a00e64b82`, including
upstream main, optional streaming, the corrected fetch backend and shared
fixed-function WGSL/empty-transfer fixes. The real Emscripten 6.0.11 build
passes without `--allow-unmodelled`; this was not a stub build.

On Windows/Chromium 145 with hardware WebGPU, a fresh streamed profile mounted
11,538 files, verified the pinned 1.20 executable and supporting module, and
rendered the game's startup settings. Clicking Play with movies disabled
produced black output and logged `D3D11: a render target drawn on the GPU
could not be read back`. Enabling the launcher's CustomDDraw / Nothing option
also produced black output and the same readback diagnostic. No JavaScript
exception, failed asset request or WebGPU validation error was observed.
The OPFS game directory remained empty. This is not a browser gameplay pass;
the browser backend's unsupported GPU readback path remains a concrete blocker.
Folder import and save/reload were not separately rerun for Siege.

All ten portable project tests pass with the MinGW runtime on PATH. Historical
Windows/macOS/iPad gameplay results do not establish browser compatibility.
Private build logs, screenshots and browser diagnostics stay ignored.
