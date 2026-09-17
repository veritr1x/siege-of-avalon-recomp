# Working on Siege of Avalon Recomp

Read README.md, CONTRIBUTING.md and docs/analysis.md before a broad change.
This repository holds only what is Siege of Avalon: Anthology's: config,
curated symbols, native replacements, smoke scripts and docs. The runtime, translator, hosts and tools are the kit in `kit/` (a
git submodule of recomp-kit); edit those in the kit's own repository and
bump the submodule here. Game files and translations are private local
inputs under ignored `original/`, `analysis/` and `build/`.

- The port plays on macOS and the iPad. docs/analysis.md records
  what the executable needs, what the kit lacks and how far each run got;
  keep it current rather than claiming progress in README.md.
- The pinned executable is the community 1.19 patch's own `Siege.exe`, in
  `original/patched` with the patch unpacked over the install. Do not bypass
  the hash: another build is another `game.toml`. Build with
  `--allow-unmodelled`: its listings decode padding as code.
- This is a Delphi program, not a Visual C++ one. Expect Unicode (`W`)
  imports throughout, exceptions raised through `RaiseException` and
  `RtlUnwind` as ordinary control flow, a `.tls` directory, delay-loaded
  imports in `.didata`, and the code split across `.text` and `.itext`
  (the entry point is in `.itext`). Kit work for these belongs in the kit.
- Keep changes focused; preserve unrelated local work and player saves.
- Never replace 32-bit guest addresses with host pointers. Addresses belong
  in `game.toml` `[hooks]` and `globals.toml`, never in kit code. A sentinel
  address (see game.toml) is replaced only by an address verified in the
  listings; `tests/test_game_config.py` keeps the unverified ones in the
  sentinel range.
- Edit translation rules in the kit, not `build/recomp/gen/`. Regenerate with
  `tools/build.py --regenerate` after changing the translator.
- Native code builds only through `tools/build.py` and `tools/test.py`, never
  by invoking compilers directly. Format kit sources with
  `.venv/bin/python kit/tools/format.py --write`.
- Run relevant suites from docs/testing.md and report exactly which checks
  ran; a configure or a listing export does not establish that the game runs.
- Do not commit game assets, generated code, binaries, credentials, personal
  saves or run logs.
- Keep setup/build instructions reproducible from a clean checkout. Update
  the changelog for user-visible behaviour.
