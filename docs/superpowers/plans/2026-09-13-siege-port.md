# Siege of Avalon: Anthology Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Take `Siege.exe` (the 2021 Delphi build in the GOG installer) from "surveyed, nothing translates" to a game that plays on every platform the kit builds for today: macOS and iPad first, then Linux and Windows, each with a smoke script that proves the main menu and the first map.

**Architecture:** The game repository (`siege-of-avalon-recomp`) holds only config, tests, smoke scripts and docs; every fix lands in the kit (`recomp-kit`) on a branch the submodule follows, then merges to kit `main` when the game boots, the way `majesty-translator` did. The work is in six phases: (A) the translator learns the Delphi runtime's instructions so the executable translates and compiles; (B) the runtime learns what a Delphi VCL program needs before its first window (TLS directory, run-time module loading, the Unicode API, resources, structured exception handling); (C) the game boots on macOS through the headless and smoke hosts with FMOD audio, then plays in the app; (D) the iPad build; (E) Linux and Windows hosts and packages; (F) documentation and pins. Phases A and B are unit-tested without the game; C onwards is run-report driven, and each such task names the command, the log it reads and the rule for turning each finding into a fix.

**Tech Stack:** Python 3.9 (`tools/recomp/translate.py`, pefile, capstone, Unicorn for the instruction tests), C++17 kit runtime (`runtime/`, `dx/`, `host/`), CMake presets `macos`, `ios`, `linux`, `windows`, SDL3, Metal and Vulkan through `host/gpu`, Ghidra 12.1.3 for listings.

**Spec:** `docs/analysis.md` in this repository (the survey and run log this plan argues from) and the kit's `docs/superpowers/specs/2026-09-13-recomp-kit-design.md` (sections 4.3 runtime, 4.4 win32, 6 cross-platform rules, 9 milestones). Task 12 writes the one new spec this plan needs, `kit/docs/superpowers/specs/2026-09-14-seh-design.md`.

## Global Constraints

- Platforms in scope: the four CMake presets the kit has (`macos`, `ios`, `linux`, `windows`). Android is not in scope: the kit has no Android preset or packager (kit spec milestone M4); do not start it here.
- Kit work happens in `~/Documents/Tests/recomp-kit` on branch `siege-delphi`, created from `main` at `31f0f24`. The game repository's submodule `kit/` follows that branch (`git -C kit checkout siege-delphi`, commit the pointer) and is re-pinned to `main` in Task 24 once the branch merges.
- Nothing under the kit's `runtime/`, `dx/`, `host/` or `platform/` may name a game; `kit/tests/test_game_literals.py` enforces it and `kit/tools/check_game_literals.py` runs in kit CI. "Delphi" is a compiler, not a game, and may be named in comments.
- Guest addresses belong in `game.toml` `[hooks]` and `globals.toml`, never in kit code. Sentinels in `game.toml` are replaced only by addresses verified in `analysis/decompiled/Siege.exe/functions/*.asm`; `tests/test_game_config.py` keeps unverified ones in `0x00d06e00`-`0x00d06fff`.
- Environment switches are `RECOMP_<NAME>` read through `recomp_env("<NAME>")` (`kit/platform/os.h`); no game-named or legacy switch names.
- Native code builds only through `tools/build.py` and `tools/test.py` from the game repository root; never invoke compilers directly. Redirect build output to a file under `build/` and check the exit code; never pipe `build.py` into `grep`.
- Format kit sources before every kit commit: `.venv/bin/python kit/tools/format.py --write` (from the game repo) or `.venv/bin/python tools/format.py --write` (from the kit).
- Commits in every repository under `~/Documents/Tests` carry the `veritr1x` identity (the global `includeIf` sets it) and no `Co-Authored-By` trailer. Committed files carry no personal strings: write `<TEAM_ID>` for the Apple team, `<DEVICE_ID>` for the iPad, `~` for the home directory.
- Every task that changes what the game does ends by appending a dated entry to `docs/analysis.md`'s run log and a line to `CHANGELOG.md` in the game repository; a configure, a listing export or a passing unit test is never reported as "the game runs".
- The pinned executable stays `Siege.exe` SHA-256 `0c028b582632129a43ea67da6040ecc5d78a14e3bba06fcd2e4071b06a9ebd5b`; never bypass the hash.
- Test commands and where their output goes: `.venv/bin/python -m pytest -q kit/tools/recomp/tests/<file>` for translator suites (no game needed); `.venv/bin/python tools/test.py --native` for the native suites (`nogame` label everywhere, `game` label needs `original/gog`); `.venv/bin/python -m pytest -q tests` for the game config. Logs under `build/`.

---

## Phase A: the executable translates and compiles

### Task 1: Translator rules for the Delphi runtime's atomics (`CMPXCHG`, `XADD`, `PAUSE`, `CMC`, `STMXCSR`)

**Files:**
- Modify: `kit/tools/recomp/translate.py` (the `_emit` dispatcher, next to the `XCHG` rule at line 1863; the `NOP`/`WAIT` rule at line 2162)
- Test: `kit/tools/recomp/tests/test_translate_insns.py` (append to `CASES`, line 201)

**Interfaces:**
- Consumes: `self.rmw(op, size, L)`, `read_op(op, size)`, `write_op(op, size, expr)`, `operand_size(ops)`, `self.flags_arith(kind, size, live)`, `hexlit`, `mask_of` as used by the `ADD`/`CMP` rule at `translate.py:1928-1945`. The listing parser already splits `CMPXCHG.LOCK` into `mnem="CMPXCHG"`, `rep="LOCK"` (`parse_listing_text`, line 458), so `ins.rep == "LOCK"` is the prefix and carries no meaning on a single-threaded guest.
- Produces: emitted C for the five mnemonics; no new runtime helpers.

- [ ] **Step 1: Write the failing Unicorn cases**

Append to `CASES` in `kit/tools/recomp/tests/test_translate_insns.py`, before the closing `]`:

```python
    Case("CMPXCHG stores when EAX equals the destination and loads EAX otherwise", 0x0D01F000,
         [(0x0, "CMPXCHG.LOCK dword ptr [ECX],EDX"), (0x4, "SETZ BL"), (0x7, "RET")],
         "F0 0F B1 11  0F 94 C3  C3", cmpxchg_setup),
    Case("XADD exchanges and adds", 0x0D020000,
         [(0x0, "XADD.LOCK dword ptr [ECX],EDX"), (0x4, "RET")],
         "F0 0F C1 11  C3", lambda rng: {"regs": rand_regs(rng, ECX=SCRATCH + 0x40),
                                          "mem": [(SCRATCH + 0x40, struct.pack("<I", rng.getrandbits(32)))]}),
    Case("PAUSE is a hint and changes nothing", 0x0D021000,
         [(0x0, "PAUSE"), (0x2, "RET")],
         "F3 90  C3", lambda rng: {"regs": rand_regs(rng)}),
    Case("CMC complements the carry", 0x0D022000,
         [(0x0, "STC"), (0x1, "CMC"), (0x2, "SETC BL"), (0x5, "CMC"), (0x6, "SETC CL"), (0x9, "RET")],
         "F9  F5  0F 92 C3  F5  0F 92 C1  C3", lambda rng: {"regs": rand_regs(rng)}),
    Case("STMXCSR stores the default MXCSR", 0x0D023000,
         [(0x0, "STMXCSR dword ptr [ECX]"), (0x3, "RET")],
         "0F AE 19  C3", lambda rng: {"regs": rand_regs(rng, ECX=SCRATCH + 0x80)}),
```

and, above `CASES`, the setup that covers both outcomes of the compare:

```python
def cmpxchg_setup(rng):
    """Half the runs make EAX equal the destination, so both paths are checked."""
    mem = rng.getrandbits(32)
    eax = mem if rng.random() < 0.5 else rng.getrandbits(32)
    return {"regs": rand_regs(rng, ECX=SCRATCH + 0x40, EAX=eax),
            "mem": [(SCRATCH + 0x40, struct.pack("<I", mem))]}
```

- [ ] **Step 2: Run the suite to see the five cases fail**

Run: `cd ~/Documents/Tests/recomp-kit && git checkout -b siege-delphi main && .venv/bin/python -m pytest -q tools/recomp/tests/test_translate_insns.py -k "CMPXCHG or XADD or PAUSE or CMC or STMXCSR" 2>&1 | tail -15`
Expected: 5 failures, each with `TranslateError: ... unhandled mnemonic <NAME>`.

- [ ] **Step 3: Add the rules**

In `_emit`, directly after the `XCHG` rule (the block ending `return L` at line 1870), add:

```python
        if m == "CMPXCHG":
            # Single-threaded guest: LOCK is a no-op. Flags are those of CMP EAX,dst.
            size = operand_size(ops)
            dst, src = ops
            dst = self.rmw(dst, size, L)
            L.append("uint32_t a_ = c->r[0] & %s, b_ = %s;" % (hexlit(mask_of(size)), read_op(dst, size)))
            L.append("uint64_t rf_ = (uint64_t)a_ - b_;")
            L.append("uint32_t r_ = (uint32_t)rf_ & %s;" % hexlit(mask_of(size)))
            L.append("if (a_ == b_) { %s } else { %s }"
                     % (write_op(dst, size, read_op(src, size)),
                        write_op(Op("reg", reg=0, size=size, part=None), size, "b_")))
            L += self.flags_arith("sub", size, live)
            return L

        if m == "XADD":
            size = operand_size(ops)
            dst, src = ops
            dst = self.rmw(dst, size, L)
            L.append("uint32_t a_ = %s, b_ = %s;" % (read_op(dst, size), read_op(src, size)))
            L.append("uint64_t rf_ = (uint64_t)a_ + b_;")
            L.append("uint32_t r_ = (uint32_t)rf_ & %s;" % hexlit(mask_of(size)))
            L.append(write_op(src, size, "a_"))
            L.append(write_op(dst, size, "r_"))
            L += self.flags_arith("add", size, live)
            return L

        if m == "CMC":
            return ["c->eflags_cf = !c->eflags_cf;"]

        if m == "STMXCSR":
            # No SSE arithmetic is translated, so MXCSR is always the reset value.
            return ["wr32(%s, 0x1f80u);" % addr_expr(ops[0])]
```

and change the `NOP`/`WAIT` line at 2162 to `if m in ("NOP", "WAIT", "PAUSE"):`.

Check that `Op("reg", reg=0, size=size, part=None)` matches how `parse_reg` builds register operands (line 274-277: `Op("reg", reg=r[0], size=r[1], part=r[2])`); if `part` must be `"low"`/`"high"` for 8-bit, use `part=None` only for 16 and 32 bits and raise `TranslateError("8-bit CMPXCHG")` for 8, which the Delphi runtime does not use.

- [ ] **Step 4: Run the suite to see the five cases pass, and the rest still pass**

Run: `.venv/bin/python -m pytest -q tools/recomp/tests/test_translate_insns.py 2>&1 | tail -3`
Expected: all passed (the count before this task plus 5).

- [ ] **Step 5: Format and commit**

```bash
.venv/bin/python tools/format.py --write
git add tools/recomp/translate.py tools/recomp/tests/test_translate_insns.py
git commit -m "Translator: CMPXCHG, XADD, PAUSE, CMC and STMXCSR for a runtime that spins and swaps"
```

### Task 2: Translator rules for the Delphi runtime's x87 forms (`FCLEX`, `FLDLN2`, `FLDL2E`, `FBSTP tword`)

**Files:**
- Modify: `kit/tools/recomp/translate.py` (`PTR_SIZE` and `MEM_RE` at lines 258-264; `emit_x87` constants at lines 2292-2297; the `FNCLEX` rule at line 2164)
- Modify: `kit/runtime/x86.h` (add `wrbcd80` next to `wrf80` at line 121)
- Test: `kit/tools/recomp/tests/test_translate_insns.py`

**Interfaces:**
- Consumes: `fpush(c, v)`, `fpop(c)`, `addr_expr(op)`, `PTR_SIZE`.
- Produces: `static inline void wrbcd80(uint32_t a, double v)` in `x86.h`: stores `v` as an 18-digit packed BCD integer with the sign in bit 7 of byte 9, rounding to nearest even as `FBSTP` does under the default control word.

- [ ] **Step 1: Write the failing cases**

Append to `CASES`:

```python
    Case("FLDLN2 and FLDL2E push the x87 constants", 0x0D024000,
         [(0x0, "FLDLN2"), (0x2, "FLDL2E"), (0x4, "FSTP double ptr [ECX]"),
          (0x7, "FSTP double ptr [ECX + 0x8]"), (0xa, "RET")],
         "D9 ED  D9 EA  DD 19  DD 59 08  C3", lambda rng: {"regs": rand_regs(rng, ECX=SCRATCH + 0x100)},
         ignore=((SCRATCH + 0x100, 16),), check=check_x87_constants),
    Case("FCLEX clears the status word like FNCLEX", 0x0D025000,
         [(0x0, "FLDZ"), (0x2, "FLDZ"), (0x4, "FDIVP ST1,ST0"), (0x6, "FCLEX"),
          (0x9, "FNSTSW word ptr [ECX]"), (0xc, "FSTP double ptr [ECX + 0x8]"), (0xf, "RET")],
         "D9 EE  D9 EE  DE F9  9B DB E2  DD 39  DD 59 08  C3",
         lambda rng: {"regs": rand_regs(rng, ECX=SCRATCH + 0x100)}, ignore=((SCRATCH + 0x108, 8),)),
    Case("FBSTP stores packed BCD and pops", 0x0D026000,
         [(0x0, "FILD dword ptr [ECX + 0x10]"), (0x3, "FBSTP tword ptr [ECX]"), (0x5, "RET")],
         "DB 41 10  DF 31  C3", fbstp_setup),
```

with, above `CASES`:

```python
def check_x87_constants(native, emu):
    ln2, l2e = struct.unpack("<dd", native.mem(SCRATCH + 0x100, 16))
    assert abs(l2e - 0.6931471805599453) < 1e-15, l2e   # pushed first, popped first: ST(0) was FLDL2E
    assert abs(ln2 - 1.4426950408889634) < 1e-15, ln2

def fbstp_setup(rng):
    value = rng.randint(-999999999, 999999999)
    return {"regs": rand_regs(rng, ECX=SCRATCH + 0x200),
            "mem": [(SCRATCH + 0x210, struct.pack("<i", value))]}
```

If `native.mem(addr, n)` does not exist in the harness (check how `check_fptan` reads results, line ~230), use the same accessor `check_fptan` uses.

Note the FSTP order in the constants case: after `FLDLN2; FLDL2E`, ST(0) is log2(e), so the first `FSTP` writes 1.4426... at `[ECX]` and the second writes ln 2 at `[ECX+8]`; make the two asserts match that (swap the names if the assertion above reads them the other way round when you run it).

- [ ] **Step 2: Run and see the three cases fail**

Run: `.venv/bin/python -m pytest -q tools/recomp/tests/test_translate_insns.py -k "FLDLN2 or FCLEX or FBSTP" 2>&1 | tail -8`
Expected: 3 failures: `unhandled x87 mnemonic FLDLN2`, `unhandled x87 mnemonic FCLEX`, `unparsed operand 'tword ptr [ECX]'`.

- [ ] **Step 3: Implement**

`translate.py` line 258-264:

```python
PTR_SIZE = {"byte": 8, "word": 16, "dword": 32, "qword": 64, "tword": 80,
            "float": 32, "double": 64, "extended double": 80}

MEM_RE = re.compile(
    r"^(?:(byte|word|dword|qword|tword|float|double|extended double) ptr )?"
    r"(?:([CDEFGS]S):)?"
    r"\[([^\]]*)\]$")
```

Line 2164: `if m in ("FNCLEX", "FCLEX"):`.

In `emit_x87`, after the `FLDPI` rule:

```python
        if m == "FLDLN2":
            return ["fpush(c, 0.69314718055994530942);"]
        if m == "FLDL2E":
            return ["fpush(c, 1.44269504088896340736);"]
        if m == "FBSTP":
            return ["wrbcd80(%s, fpop(c));" % addr_expr(ops[0])]
```

`runtime/x86.h`, after `wrf80` (line 121):

```c
/* FBSTP: 18 packed BCD digits, little-endian, sign in bit 7 of byte 9. Rounds
 * to nearest even as the default control word does. Out-of-range values store
 * the BCD indefinite (0xffff c000 0000 0000 0000), as the hardware does. */
static inline void wrbcd80(uint32_t a, double v) {
    uint8_t out[10];
    memset(out, 0, sizeof out);
    double r = nearbyint(v);
    if (!(fabs(r) < 1e18)) {
        static const uint8_t indefinite[10] = {0, 0, 0, 0, 0, 0, 0, 0xc0, 0xff, 0xff};
        memcpy(g_mem + a, indefinite, 10);
        return;
    }
    uint64_t m = (uint64_t)fabs(r);
    for (int i = 0; i < 9; ++i) {
        uint8_t lo = (uint8_t)(m % 10); m /= 10;
        uint8_t hi = (uint8_t)(m % 10); m /= 10;
        out[i] = (uint8_t)(hi << 4 | lo);
    }
    if (r < 0 || (r == 0 && signbit(v)))
        out[9] = 0x80;
    memcpy(g_mem + a, out, 10);
}
```

`x86.h` already includes `<math.h>` for `x87_indefinite`'s neighbours; if `nearbyint`/`signbit` are missing, add `#include <math.h>` at the top with the other includes.

- [ ] **Step 4: Run the whole instruction suite**

Run: `.venv/bin/python -m pytest -q tools/recomp/tests/test_translate_insns.py 2>&1 | tail -3`
Expected: all passed.

- [ ] **Step 5: Format and commit**

```bash
.venv/bin/python tools/format.py --write
git add runtime/x86.h tools/recomp/translate.py tools/recomp/tests/test_translate_insns.py
git commit -m "Translator: FCLEX, FLDLN2, FLDL2E and FBSTP for Delphi's Extended math"
```

### Task 3: The four indirect jumps that are not tables

**Files:**
- Modify: `kit/tools/recomp/translate.py` (the jump-table decoder and its gate at lines ~2990-3013)
- Test: `kit/tools/recomp/tests/test_jumptables.py`

**Interfaces:**
- Consumes: the four sites the run log names: `0x008b97e9` in `fn_008b97de`, `0x00972b58` in `fn_00972b51`, `0x0098542a` in `fn_00985423`, `0x00ad6996` in `fn_00ad698f`. Read them first: `sed -n '/^008b97/p' analysis/decompiled/Siege.exe/functions/008b97de.asm` and the same for the other three.
- Produces: a decoder rule, `is_table_site(insn)`, that a constant-displacement indirect jump is a table only when its displacement lies inside the image's data ranges; otherwise the jump is emitted as a computed `recomp_jump`.

- [ ] **Step 1: Read the four sites and record their shape**

Run the four `sed` commands above. Expected: each is `JMP dword ptr [<reg> + <small displacement>]` or `JMP dword ptr [<reg>*4 + <displacement>]` where the displacement (`-0x75`, `0x10097`, `-0x68`, `0x100ae`) is not an image address (the image spans `0x00800000`-`0x00d07000`). Write the four exact lines into the test's docstring in Step 2.

- [ ] **Step 2: Write the failing test**

Open `kit/tools/recomp/tests/test_jumptables.py`, find how an existing test builds a listing with a table site (search for `"JMP dword ptr ["` in that file) and add a test in that style:

```python
def test_indirect_jump_through_a_structure_field_is_not_a_table(tmp_path):
    """A JMP through [reg + small displacement] (Siege.exe 008b97e9: `JMP dword ptr [EBX + -0x75]`,
    and three siblings) is a computed jump, not a switch table: the displacement is not an
    image address, so there is nothing to decode. The translator must emit recomp_jump and
    must not count the site as a table that decoded nothing."""
    listing = ("0d010000  MOV EBX,dword ptr [ESP + 0x4]\n"
               "0d010004  JMP dword ptr [EBX + -0x75]\n")
    result = translate_listing(listing, image=NoImage())   # the helper the neighbouring tests use
    assert "recomp_jump(c," in result.c_source
    assert result.table_sites == []
```

Adapt `translate_listing`, `NoImage` and the result fields to the helper names the file actually uses (read the first 80 lines of the file); the assertions are the contract.

- [ ] **Step 3: Run it to see it fail**

Run: `.venv/bin/python -m pytest -q tools/recomp/tests/test_jumptables.py -k structure_field 2>&1 | tail -5`
Expected: FAIL, either with the gate's `table sites decoded nothing` text or with the site listed in `table_sites`.

- [ ] **Step 4: Implement the rule**

In `translate.py`, where the translator decides an indirect `JMP` with a constant displacement is a table site (search for the code that produces the "decoded no entries at all" message, near line 3005), add before it:

```python
        # A displacement that is not an address inside the image is a field
        # offset, not a table: `JMP dword ptr [EBX + -0x75]` continues through a
        # pointer the guest stored in a structure. Emit a computed jump.
        if not image.in_data_ranges(disp):
            return None  # not a table site; the emitter's recomp_jump path applies
```

and give `Image` (line ~613) the helper if it does not have one:

```python
    def in_data_ranges(self, addr):
        return any(lo <= addr < hi for lo, hi, _name in self.data_ranges)
```

Then make the emitter's indirect-jump path (`emit_indirect_jump`, line 2185) fall through to `recomp_jump(c, <computed address>)` when the decoder returned `None`.

- [ ] **Step 5: Run the table and driver suites**

Run: `.venv/bin/python -m pytest -q tools/recomp/tests/test_jumptables.py tools/recomp/tests/test_translate_driver.py 2>&1 | tail -3`
Expected: all passed.

- [ ] **Step 6: Format and commit**

```bash
.venv/bin/python tools/format.py --write
git add tools/recomp/translate.py tools/recomp/tests/test_jumptables.py
git commit -m "Translator: an indirect jump through a field offset is a computed jump, not a table"
```

### Task 4: Siege.exe translates end to end and its C compiles

**Files:**
- Modify: `kit/tools/recomp/translate.py:3314` (the report writes `"image_base": "00400000"` as a literal; write `"%08x" % image.base`)
- Modify: game repo `docs/analysis.md` (run log), `CHANGELOG.md`, `kit` submodule pointer

**Interfaces:**
- Consumes: Tasks 1-3 on kit branch `siege-delphi`.
- Produces: `build/recomp/translate-report.json`, `build/recomp/gen/` with the translation, and a `pop_headless` binary that links against it.

- [ ] **Step 1: Point the submodule at the branch and translate**

```bash
cd ~/Documents/Tests/siege-of-avalon-recomp
git -C kit fetch ~/Documents/Tests/recomp-kit siege-delphi && git -C kit checkout FETCH_HEAD
.venv/bin/python tools/build.py --regenerate --target headless --jobs 8 > build/regenerate.log 2>&1; echo "exit $?"
```

Expected on the first run: the translator gets past both gates measured in the run log (no `table sites decoded nothing`, no `literal dispatch targets are not entry points`). If `failed:` lines remain, they name mnemonics; each is a Task 1/2-shaped rule (a Unicorn case, a rule, a commit) and this task loops until `fn_... failed:` no longer appears. If `dispatches to ... which is not an entry point` remains for a target that is in `functions.tsv` and has no `failed:` line, print `grep -n "<target>" build/regenerate.log` and the target's `.asm`: the translator rejected it in `accepts()` (line 2666) for a reason it did not print; run `.venv/bin/python kit/tools/recomp/translate.py --out build/probe --game "$PWD" --only <target>` to see the error.

- [ ] **Step 2: Compile time and compiler errors**

The generated chunks compile with the host clang. Expected first-compile findings and their fixes:
- an error inside `chunk_*.c` names a construct the emitter produced (for instance a `switch` with duplicate case labels from a table with repeated entries): fix the emitter, add a case to `test_translate_insns.py` or `test_jumptables.py`, regenerate;
- a link error `undefined symbol: fn_<addr>`: an entry the driver named but did not emit; the fix is in `translate.py`'s `entry_names` (line 2983) and is a bug, not a gap.
Record every fix as a kit commit on `siege-delphi`.

- [ ] **Step 3: Confirm the report**

Run: `python3 -c "import json; r=json.load(open('build/recomp/translate-report.json')); print({k: r[k] for k in ('entry_points','image_base','unsupported_instructions','unresolved_targets') if k in r})"`
Expected: `image_base` is `00800000`; `entry_points` near 8,906 plus interior entries; unsupported and unresolved counts are 0.

- [ ] **Step 4: Record and commit in both repositories**

Append to `docs/analysis.md` under "Run log" a dated entry with: the kit commits it took, the report's numbers, the wall time of `--regenerate`, and the size of `build/recomp/gen`. Add a `CHANGELOG.md` line: "The executable translates and compiles; `pop_headless` links against the translation (kit `siege-delphi` <sha>)".

```bash
cd ~/Documents/Tests/siege-of-avalon-recomp
git add kit docs/analysis.md CHANGELOG.md
git commit -m "Siege.exe translates and compiles: the kit's siege-delphi branch, run log"
```

---

## Phase B: what a Delphi VCL program needs before its first window

### Task 5: The loader processes the TLS directory

**Files:**
- Modify: `kit/runtime/loader.cpp` (after the section map, before IAT binding, near line 400; `loader_init_context` at line 427)
- Modify: `kit/runtime/loader.h` (new accessors)
- Modify: `kit/runtime/kernel32.cpp` (`thread_prepare_context`, line 3010: give each new thread its TLS block; `k_TlsAlloc` at 1640 must skip the reserved index)
- Test: `kit/runtime/tests/runtime_tests.cpp` (`test_loader`, line 191, under the `game` label)

**Interfaces:**
- Produces in `loader.h`:
  ```cpp
  // The PE TLS directory, if the image has one. index is the slot the loader
  // reserved in every thread's TLS array; 0xffffffff when there is none.
  struct LoaderTls { uint32_t raw_start, raw_end, index_addr, callbacks, zero_fill, index; };
  const LoaderTls &loader_tls();
  // Allocates and initialises one thread's TLS block (raw data plus zero fill)
  // from the guest heap and stores its address in slot `index` of `tls_array`.
  // Returns the block, 0 when the image has no TLS directory or the heap is full.
  uint32_t loader_tls_block_for_thread(uint32_t tls_array);
  ```
- Consumes: `heap_alloc(bytes, zero, align)` (kernel32.cpp, as `thread_prepare_context` uses it), `g_tls_used[]` and `TLS_SLOTS` (kernel32.cpp / guest.h).

- [ ] **Step 1: Write the failing test**

In `runtime_tests.cpp` `test_loader()`, after the `loader_image_limit` check, add:

```cpp
    // A Delphi image carries a TLS directory; the loader reserves a slot, writes
    // its index where the image reads it, and gives the main thread a block that
    // starts with the directory's raw bytes.
    const LoaderTls &tls = loader_tls();
    if (tls.raw_end > tls.raw_start) {
        check(tls.index < TLS_SLOTS, "TLS slot %u reserved", tls.index);
        check(rd32(tls.index_addr) == tls.index, "the image's TLS index reads %u", rd32(tls.index_addr));
        uint32_t block = rd32(TLS_BASE + 4 * tls.index);
        check(block != 0, "main thread TLS block at %08x", block);
        check(memcmp(g_mem + block, g_mem + tls.raw_start, tls.raw_end - tls.raw_start) == 0,
              "the block starts with the directory's raw data");
    } else {
        check(tls.index == 0xffffffffu, "no TLS directory: no slot reserved");
    }
```

The `game` label loads `RECOMP_DEVELOPER_EXE`; against Siege.exe the first branch runs (`raw_start 0xc39000`, `raw_end 0xc39048`, `index_addr 0xbffc28`, `callbacks 0xc3a010`), against the Populous image the second.

- [ ] **Step 2: Run it to see it fail to compile**

Run: `cd ~/Documents/Tests/siege-of-avalon-recomp && .venv/bin/python tools/test.py --native -R runtime_tests > build/runtime_tests.log 2>&1; tail -5 build/runtime_tests.log`
Expected: compile error, `loader_tls` undeclared.

- [ ] **Step 3: Implement**

`loader.cpp`, after the section loop (line ~398) and before the IAT is patched:

```cpp
    // TLS directory (data directory 9): reserve a slot for the image's index.
    g_tls = LoaderTls{0, 0, 0, 0, 0, 0xffffffffu};
    uint32_t dd_tls = opt_off + 96 + 9 * 8;
    uint32_t tls_rva = rd<uint32_t>(file, dd_tls), tls_size = rd<uint32_t>(file, dd_tls + 4);
    if (tls_rva && tls_size >= 24) {
        uint32_t d = image_base + tls_rva;
        g_tls.raw_start = rd32(d + 0);
        g_tls.raw_end = rd32(d + 4);
        g_tls.index_addr = rd32(d + 8);
        g_tls.callbacks = rd32(d + 12);
        g_tls.zero_fill = rd32(d + 16);
        g_tls.index = tls_reserve_slot(); // kernel32.cpp: marks g_tls_used[i] and returns i
        wr32(g_tls.index_addr, g_tls.index);
    }
```

with, in `kernel32.cpp` next to `k_TlsAlloc`:

```cpp
uint32_t tls_reserve_slot() {
    for (uint32_t i = 0; i < TLS_SLOTS; ++i)
        if (!g_tls_used[i]) {
            g_tls_used[i] = true;
            return i;
        }
    return 0xffffffffu;
}

uint32_t loader_tls_block_for_thread(uint32_t tls_array) {
    const LoaderTls &t = loader_tls();
    if (t.index == 0xffffffffu)
        return 0;
    uint32_t raw = t.raw_end - t.raw_start;
    uint32_t block = heap_alloc(raw + t.zero_fill + 16, true, 16);
    if (!block)
        return 0;
    memcpy(g_mem + block, g_mem + t.raw_start, raw);
    wr32(tls_array + 4 * t.index, block);
    return block;
}
```

In `loader_init_context` after `memset(g_mem + TLS_BASE, 0, TLS_SLOTS * 4);` call `loader_tls_block_for_thread(TLS_BASE);`. In `thread_prepare_context` after `wr32(t->teb + 0x2c, t->tls);` call `loader_tls_block_for_thread(t->tls);`. The callback list (`g_tls.callbacks`, a null-terminated array of function pointers) is run by the entry-point caller: where the loader or host first calls `g_entry`, before it, call each callback with `(image_base, 1 /* DLL_PROCESS_ATTACH */, 0)` through `recomp_call` with a cdecl-style pushed frame and the return sentinel; Delphi images ship an empty list, so guard with `if (cb) ...` and log the count at LOGV.

- [ ] **Step 4: Run the native suite under the game**

Run: `.venv/bin/python tools/test.py --native -R runtime_tests > build/runtime_tests.log 2>&1; grep -c "\[FAIL\]" build/runtime_tests.log; tail -3 build/runtime_tests.log`
Expected: 0 failures from the new checks; the existing Populous-bound `test_wide_cursor_bound` and image-layout checks fail under this game as they do under Majesty (docs/analysis.md of majesty-recomp lists them); record them as pre-existing, not new.

- [ ] **Step 5: Format and commit (kit)**

Kit edits from here on are made in `siege-of-avalon-recomp/kit`, the submodule checkout on branch `siege-delphi`, and committed there; Task 24 fast-forwards `~/Documents/Tests/recomp-kit` from it.

```bash
.venv/bin/python kit/tools/format.py --write
git -C kit add runtime/loader.cpp runtime/loader.h runtime/kernel32.cpp runtime/tests/runtime_tests.cpp
git -C kit commit -m "Loader: the TLS directory gets a slot, its index, and a block per thread"
```

### Task 6: Run-time module loading reaches every registered shim table, and the wide module API

**Files:**
- Modify: `kit/runtime/imports.cpp` and `imports.h` (new `imports_has_dll`)
- Modify: `kit/runtime/kernel32.cpp` (`runtime_serves_module`, line 1185; new `k_LoadLibraryW`, `k_LoadLibraryExW`, `k_GetModuleHandleW`, `k_GetModuleFileNameW`, registered in the table at line ~4087)
- Modify: `kit/runtime/guest.h` and `kit/runtime/memory.cpp` (wide string helpers)
- Test: `kit/runtime/tests/runtime_tests.cpp`

**Interfaces:**
- Produces in `guest.h`:
  ```cpp
  // Read a NUL-terminated UTF-16LE guest string as UTF-8 (bounded). a == 0 yields "".
  std::string gm_wstr(uint32_t a, size_t max_chars = 0x8000);
  // Write s as UTF-16LE with a terminator, truncating to cap UTF-16 units including
  // the terminator. Returns the number of units written excluding the terminator.
  uint32_t gm_put_wstr(uint32_t a, const std::string &s, uint32_t cap);
  ```
- Produces in `imports.h`: `bool imports_has_dll(const char *dll_lower);` true when any shim table registered an entry for that DLL (case-insensitive).
- Every later "W" shim in this plan uses these two helpers and nothing else for text.

- [ ] **Step 1: Write the failing tests**

In `runtime_tests.cpp`, a new `static void test_modules_and_wide()` called from `main` next to `test_loader`:

```cpp
static void test_modules_and_wide() {
    X86 c;
    loader_init_context(&c);
    // The DirectX tables register DDRAW.dll; a guest that loads it at run time
    // must get a module and resolve DirectDrawCreate through GetProcAddress.
    uint32_t name = 0x00300000; // scratch in the arena below the image
    gm_put_str(name, "ddraw.dll", 64);
    uint32_t h = call_import(&c, "KERNEL32.dll", "LoadLibraryA", {name});
    check(h != 0, "LoadLibraryA(ddraw.dll) -> %08x", h);
    gm_put_str(name + 64, "DirectDrawCreate", 64);
    check(call_import(&c, "KERNEL32.dll", "GetProcAddress", {h, name + 64}) != 0,
          "GetProcAddress(ddraw, DirectDrawCreate) resolves");
    // The same through the wide entry points the Delphi runtime uses.
    gm_put_wstr(name + 128, "soaddraw.dll", 64);
    check(call_import(&c, "KERNEL32.dll", "LoadLibraryW", {name + 128}) == 0,
          "LoadLibraryW(soaddraw.dll): no shims, reported missing");
    gm_put_wstr(name + 128, "DDRAW.DLL", 64);
    check(call_import(&c, "KERNEL32.dll", "LoadLibraryW", {name + 128}) == h,
          "LoadLibraryW(DDRAW.DLL) returns the same module");
    check(call_import(&c, "KERNEL32.dll", "GetModuleHandleW", {name + 128}) == h, "GetModuleHandleW agrees");
    check(gm_wstr(name + 128) == "DDRAW.DLL", "gm_wstr round-trips");
    gm_put_wstr(name + 256, "abc", 3); // cap includes the terminator: two units and NUL
    check(gm_wstr(name + 256) == "ab", "gm_put_wstr truncates to the cap");
}
```

- [ ] **Step 2: Run to see it fail**

Run: `.venv/bin/python tools/test.py --native -R runtime_tests > build/runtime_tests.log 2>&1; grep -E "FAIL|error:" build/runtime_tests.log | head`
Expected: compile errors for `gm_put_wstr`/`gm_wstr`; after stubbing them, `LoadLibraryA(ddraw.dll) -> 00000000` fails because `runtime_serves_module` lists nine DLLs by name.

- [ ] **Step 3: Implement**

`memory.cpp`:

```cpp
std::string gm_wstr(uint32_t a, size_t max_chars) {
    std::string out;
    if (!a)
        return out;
    for (size_t i = 0; i < max_chars && gm_valid(a + 2 * i, 2); ++i) {
        uint32_t u = rd16(a + 2 * (uint32_t)i);
        if (!u)
            break;
        if (u >= 0xd800 && u < 0xdc00 && gm_valid(a + 2 * (i + 1), 2)) {
            uint32_t lo = rd16(a + 2 * (uint32_t)(i + 1));
            if (lo >= 0xdc00 && lo < 0xe000) {
                u = 0x10000 + ((u - 0xd800) << 10) + (lo - 0xdc00);
                ++i;
            }
        }
        if (u < 0x80) out += (char)u;
        else if (u < 0x800) { out += (char)(0xc0 | (u >> 6)); out += (char)(0x80 | (u & 0x3f)); }
        else if (u < 0x10000) { out += (char)(0xe0 | (u >> 12)); out += (char)(0x80 | ((u >> 6) & 0x3f)); out += (char)(0x80 | (u & 0x3f)); }
        else { out += (char)(0xf0 | (u >> 18)); out += (char)(0x80 | ((u >> 12) & 0x3f)); out += (char)(0x80 | ((u >> 6) & 0x3f)); out += (char)(0x80 | (u & 0x3f)); }
    }
    return out;
}

uint32_t gm_put_wstr(uint32_t a, const std::string &s, uint32_t cap) {
    if (!a || cap == 0)
        return 0;
    uint32_t n = 0;
    size_t i = 0;
    while (i < s.size() && n + 1 < cap) {
        unsigned char b = (unsigned char)s[i];
        uint32_t u; size_t len;
        if (b < 0x80) { u = b; len = 1; }
        else if ((b & 0xe0) == 0xc0) { u = b & 0x1f; len = 2; }
        else if ((b & 0xf0) == 0xe0) { u = b & 0x0f; len = 3; }
        else { u = b & 0x07; len = 4; }
        for (size_t k = 1; k < len && i + k < s.size(); ++k)
            u = (u << 6) | ((unsigned char)s[i + k] & 0x3f);
        i += len;
        if (u >= 0x10000) {
            if (n + 2 >= cap) break;
            wr16(a + 2 * n++, (uint16_t)(0xd800 + ((u - 0x10000) >> 10)));
            wr16(a + 2 * n++, (uint16_t)(0xdc00 + ((u - 0x10000) & 0x3ff)));
        } else {
            wr16(a + 2 * n++, (uint16_t)u);
        }
    }
    wr16(a + 2 * n, 0);
    return n;
}
```

`imports.cpp`: keep a `std::set<std::string>` of lower-cased DLL names filled in `imports_register`, and

```cpp
bool imports_has_dll(const char *dll_lower) {
    return registered_dlls().count(dll_lower) != 0;
}
```

`kernel32.cpp` `runtime_serves_module`: replace the fixed list with `return imports_has_dll(lower_name.c_str());` and keep the comment. Then the wide entry points, each a wrapper that converts and calls the existing shim body:

```cpp
void k_LoadLibraryW(X86 *c) {
    std::string name = gm_wstr(arg(c, 0));
    load_library_named(c, name); // factor k_LoadLibraryA's body into this: takes the name, sets EAX
}
void k_LoadLibraryExW(X86 *c) { k_LoadLibraryW(c); } // flags ignored: no DLL is really mapped
void k_GetModuleHandleW(X86 *c) {
    uint32_t a = arg(c, 0);
    if (!a) { set_eax(c, IMAGE_BASE); return; }
    get_module_handle_named(c, gm_wstr(a)); // factored from k_GetModuleHandleA the same way
}
void k_GetModuleFileNameW(X86 *c) {
    // Same answer as the A variant, re-encoded: the executable's guest path for the
    // image handle or 0, the pseudo module's name otherwise.
    std::string path = module_file_name(arg(c, 0)); // factored from k_GetModuleFileNameA
    uint32_t n = gm_put_wstr(arg(c, 1), path, arg(c, 2));
    set_eax(c, n);
}
```

and the table entries `{"KERNEL32.dll", "LoadLibraryW", 1, k_LoadLibraryW}`, `{"KERNEL32.dll", "LoadLibraryExW", 3, k_LoadLibraryExW}`, `{"KERNEL32.dll", "GetModuleHandleW", 1, k_GetModuleHandleW}`, `{"KERNEL32.dll", "GetModuleFileNameW", 3, k_GetModuleFileNameW}`.

- [ ] **Step 4: Run the native suite**

Run: `.venv/bin/python tools/test.py --native -R runtime_tests > build/runtime_tests.log 2>&1; grep -c "\[FAIL\]" build/runtime_tests.log`
Expected: no new failures; the seven new checks pass.

- [ ] **Step 5: Format and commit (kit)**

```bash
.venv/bin/python kit/tools/format.py --write
git -C kit add runtime/imports.cpp runtime/imports.h runtime/kernel32.cpp runtime/guest.h runtime/memory.cpp runtime/tests/runtime_tests.cpp
git -C kit commit -m "Runtime: LoadLibrary serves every registered table; wide string helpers and the W module API"
```

### Task 7: The wide kernel32 layer (files, profiles, synchronisation, locale, version probes)

**Files:**
- Create: `kit/runtime/kernel32_wide.cpp` (every `W` entry below as a wrapper over the A shim's factored body, plus the handful with no A twin)
- Modify: `kit/runtime/kernel32.cpp` (factor bodies the wrappers call: `create_file_named`, `find_first_named`, `get_file_attributes_named`, `delete_file_named`, `create_directory_named`, ... one per A shim touched; register the new table from `imports_init`)
- Modify: `kit/runtime/CMakeLists.txt` (add `kernel32_wide.cpp`)
- Test: `kit/runtime/tests/runtime_tests.cpp` (`test_kernel32_wide`)

**Interfaces:**
- Consumes: `gm_wstr`, `gm_put_wstr` (Task 6); the A shims' bodies.
- Produces: shims for, grouped by what they share:
  - files: `CreateFileW` (7), `FindFirstFileW` (2), `FindNextFileW` (2), `GetFullPathNameW` (4), `GetFileAttributesW` (1), `GetFileAttributesExW` (3), `SetFileAttributesW` (2), `DeleteFileW` (1), `CopyFileW` (3), `CreateDirectoryW` (2), `RemoveDirectoryW` (1), `GetSystemDirectoryW` (2), `GetVolumeInformationW` (8), `GetDriveTypeW` (1), `GetLogicalDriveStringsW` (2), `GetDiskFreeSpaceW` (5), `QueryDosDeviceW` (3). `WIN32_FIND_DATAW` is the A structure with the two name fields widened: `cFileName` at offset 44 (260 UTF-16 units), `cAlternateFileName` at 564 (14 units), total 592 bytes; convert field by field.
  - profile files: `GetPrivateProfileStringW` (6), `WritePrivateProfileStringW` (4): wrap the A bodies (the kit has `GetPrivateProfileStringA`? check `grep -n PrivateProfile kit/runtime/kernel32.cpp`; if absent, implement the pair over the file seam here: read the whole `.ini` through the guest file path, parse `[section]`/`key=value`, write back the changed file, so `siege.ini` lands in the profile's write tier).
  - synchronisation and mappings: `CreateEventW` (4), `CreateMutexW` (3), `OpenMutexW` (3), `CreateFileMappingW` (6): the name argument converted, the A body called.
  - text and time: `lstrlenW` (1), `lstrcatW` (2), `FormatMessageW` (7: only `FORMAT_MESSAGE_FROM_SYSTEM` with a code, answer "Error <code>"), `OutputDebugStringW` (1, LOGV), `GetDateFormatW` (6, `yyyy-MM-dd` regardless of the picture), `FileTimeToLocalFileTime` (2, identity), `FileTimeToSystemTime` (2), `FileTimeToDosDateTime` (3).
  - locale: `GetThreadLocale` (0) and `SetThreadLocale` (1) over one process-wide LCID `0x0409`, `EnumSystemLocalesW` (2: call the guest callback once with `L"00000409"` through `recomp_call`), `EnumCalendarInfoW` (4: callback once with `L"1"`), `GetCPInfoExW` (3: code page 1252, MaxCharSize 1), `GetUserDefaultUILanguage` and `GetSystemDefaultUILanguage` (0, `0x0409`), `IsDBCSLeadByteEx` (2, false), `GetConsoleCP`/`GetConsoleOutputCP` (0, 437).
  - process and version: `GetCommandLineW` (0: the A answer widened into a static guest buffer), `GetStartupInfoW` (1: zero the 68-byte structure, `cb = 68`), `VerifyVersionInfoW` (3, TRUE) and `VerSetConditionMask` (cdecl, 64-bit in EDX:EAX: return `ConditionMask | (Condition << (7 * bit_index(TypeMask)))`, Windows' arithmetic), `GetCurrentProcessId` (0, 1), `IsDebuggerPresent` (0, 0), `SwitchToThread` (0, 0), `MulDiv` (3), `VirtualProtect` (4, TRUE, old protection `0x40`), `VirtualQuery` (3) and `VirtualQueryEx` (4): fill `MEMORY_BASIC_INFORMATION` (28 bytes) for the arena range that contains the address: image, heap, stack, else free; `SetErrorMode` (1, returns 0), `GlobalAddAtomW`/`GlobalFindAtomW`/`GlobalDeleteAtom` over a `std::map<std::string, uint16_t>` starting at `0xc000`, `WaitForMultipleObjectsEx` (5: the 4-argument body with the alertable flag ignored).
  - resources: `FindResourceW` (3), `LoadResource` (2), `LockResource` (1), `SizeofResource` (2), `FreeResource` (1), `EnumResourceNamesW` (4): a reader over the loaded image's resource directory (`.rsrc`, data directory 2). `FindResourceW(hModule, name, type)`: `name` and `type` are either integer IDs (high word zero) or `L"#123"`/string names; walk type → name → language (first entry) and return the address of the `IMAGE_RESOURCE_DATA_ENTRY`; `LoadResource` returns `image_base + DataEntry.OffsetToData`; `LockResource` is identity; `SizeofResource` reads `DataEntry.Size`. `EnumResourceNamesW` calls the guest callback for each name under the type through `recomp_call`. Put this in its own file `kit/runtime/resources.cpp` with the header `kit/runtime/resources.h` (`uint32_t resource_find(uint32_t type_id_or_name, uint32_t name_id_or_name, std::string *why)`), because user32's `LoadStringW`, `LoadIconW`, `LoadBitmapW` and version.dll's `GetFileVersionInfoW` (Task 9, Task 10) read the same directory.

- [ ] **Step 1: Write the failing tests**

`test_kernel32_wide()` in `runtime_tests.cpp`, run under the `game` label (it reads the developer game directory):

```cpp
static void test_kernel32_wide() {
    X86 c;
    loader_init_context(&c);
    uint32_t s = 0x00300000;
    // A file that exists in every supported game directory: the executable itself.
    gm_put_wstr(s, RECOMP_EXECUTABLE, 128);
    uint32_t attrs = call_import(&c, "KERNEL32.dll", "GetFileAttributesW", {s});
    check(attrs != 0xffffffffu && !(attrs & 0x10), "GetFileAttributesW(exe) = %08x", attrs);
    uint32_t h = call_import(&c, "KERNEL32.dll", "CreateFileW", {s, 0x80000000u, 1, 0, 3, 0x80, 0});
    check(h != 0xffffffffu, "CreateFileW opens the executable");
    call_import(&c, "KERNEL32.dll", "CloseHandle", {h});
    // Find: the wide record's name field is UTF-16 at offset 44.
    gm_put_wstr(s, "*.exe", 128);
    uint32_t fd = s + 0x1000;
    uint32_t fh = call_import(&c, "KERNEL32.dll", "FindFirstFileW", {s, fd});
    check(fh != 0xffffffffu, "FindFirstFileW(*.exe)");
    std::string found = gm_wstr(fd + 44);
    check(found.size() > 4 && found.substr(found.size() - 4) == ".exe", "found %s", found.c_str());
    call_import(&c, "KERNEL32.dll", "FindClose", {fh});
    // Profile strings round-trip through the write tier.
    gm_put_wstr(s, "Settings", 64); gm_put_wstr(s + 128, "Windowed", 64);
    gm_put_wstr(s + 256, "1", 64); gm_put_wstr(s + 384, "siege-test.ini", 64);
    check(call_import(&c, "KERNEL32.dll", "WritePrivateProfileStringW", {s, s + 128, s + 256, s + 384}) == 1,
          "WritePrivateProfileStringW");
    gm_put_wstr(s + 512, "0", 64);
    uint32_t n = call_import(&c, "KERNEL32.dll", "GetPrivateProfileStringW",
                             {s, s + 128, s + 512, s + 0x800, 64, s + 384});
    check(n == 1 && gm_wstr(s + 0x800) == "1", "GetPrivateProfileStringW reads back %s", gm_wstr(s + 0x800).c_str());
    // Locale answers the Delphi runtime accepts.
    check(call_import(&c, "KERNEL32.dll", "GetThreadLocale", {}) == 0x0409, "GetThreadLocale");
    check(call_import(&c, "KERNEL32.dll", "GetUserDefaultUILanguage", {}) == 0x0409, "GetUserDefaultUILanguage");
    // Resources: the version resource (type 16, name 1) exists in the pinned image.
    uint32_t r = call_import(&c, "KERNEL32.dll", "FindResourceW", {0, 1, 16});
    check(r != 0, "FindResourceW(VS_VERSION_INFO)");
    uint32_t size = call_import(&c, "KERNEL32.dll", "SizeofResource", {0, r});
    uint32_t data = call_import(&c, "KERNEL32.dll", "LoadResource", {0, r});
    check(size > 0x34 && data != 0 && rd16(data + 4) == 0xfeef04bd >> 16 ? true : rd32(data + 8) == 0xfeef04bdu,
          "the loaded resource is a VS_VERSIONINFO (size %u)", size);
}
```

(Simplify the last check to `rd32(data + 8) == 0xfeef04bdu`: the `dwSignature` of `VS_FIXEDFILEINFO` sits after the 6-byte header and the 32-byte `L"VS_VERSION_INFO"` key plus padding, so compute its offset as `(6 + 32 + 3) & ~3 = 40` and check `rd32(data + 40) == 0xfeef04bdu`.)

- [ ] **Step 2: Run to see it fail**

Run: `.venv/bin/python tools/test.py --native -R runtime_tests > build/runtime_tests.log 2>&1; grep "no trampoline\|FAIL" build/runtime_tests.log | head`
Expected: `no trampoline for KERNEL32.dll!GetFileAttributesW` and the others.

- [ ] **Step 3: Implement, group by group**

Work in the order of the interface list, running the test after each group. Each wrapper is the same three lines: read wide arguments with `gm_wstr`, call the factored body, write wide outputs with `gm_put_wstr`. The factoring of an A shim is mechanical: move its body into `static void <name>_named(X86 *c, const std::string &name, ...)` and make the A shim call it with `gm_str(arg(c, i))`. Register every new entry in a `static const ImportShim g_kernel32_wide[]` table in `kernel32_wide.cpp` and call `imports_register` on it from `imports_init`.

- [ ] **Step 4: Run the suite; expect all new checks green**

Run: `.venv/bin/python tools/test.py --native -R runtime_tests > build/runtime_tests.log 2>&1; grep -c "\[FAIL\]" build/runtime_tests.log`

- [ ] **Step 5: Format and commit (kit)**

```bash
.venv/bin/python kit/tools/format.py --write
git -C kit add runtime/kernel32_wide.cpp runtime/resources.cpp runtime/resources.h runtime/kernel32.cpp runtime/CMakeLists.txt runtime/tests/runtime_tests.cpp
git -C kit commit -m "Runtime: the wide kernel32 surface a Delphi runtime starts with, and a resource directory reader"
```

### Task 8: The smaller DLLs the Delphi runtime links (oleaut32, advapi32 W, version W, comctl32, winspool, netapi32, msvcrt, shfolder, shell32, ole32)

**Files:**
- Create: `kit/runtime/oleaut32.cpp`, `kit/runtime/comctl32.cpp`, `kit/runtime/misc_dlls.cpp` (winspool.drv, netapi32, msvcrt, shfolder, shell32's `Shell_NotifyIconW`)
- Modify: the file that owns the ADVAPI32 registry table and the version.dll table today (`grep -ln '"ADVAPI32.dll"\|"VERSION.dll"' kit/runtime/*.cpp kit/dx/*.cpp`) for the W twins; `kit/runtime/misc.cpp` or wherever `ole32` lives for `OleInitialize`, `OleUninitialize`, `CoInitializeEx`, `CoTaskMemAlloc`, `CoTaskMemFree`, `IsEqualGUID`
- Modify: `kit/runtime/CMakeLists.txt`
- Test: `kit/runtime/tests/runtime_tests.cpp` (`test_delphi_dlls`)

**Interfaces:**
- `oleaut32`: `VariantInit` (1: zero the 16-byte VARIANT), `VariantClear` (1: free a `VT_BSTR` (8) payload through the BSTR allocator, zero), `VariantCopy` (2), `VariantCopyInd` (2), `VariantChangeType` (4: support `VT_I4`(3) ↔ `VT_BSTR`(8) ↔ `VT_R8`(5) ↔ `VT_BOOL`(11); return `DISP_E_TYPEMISMATCH` `0x80020005` otherwise), `SysAllocStringLen` (2), `SysReAllocStringLen` (3), `SysFreeString` (1): a BSTR is a guest heap block whose 4-byte length prefix (bytes) precedes the UTF-16 text; `SafeArrayCreate` (3), `SafeArrayGetLBound`/`SafeArrayGetUBound` (3), `SafeArrayGetElement`/`SafeArrayPutElement` (3), `SafeArrayPtrOfIndex` (3): one-dimensional arrays over the guest heap with the 24-byte `SAFEARRAY` header plus one `SAFEARRAYBOUND`; `GetErrorInfo` (2: `S_FALSE` 1, no error object).
- `advapi32` W: `RegOpenKeyExW` (5), `RegCreateKeyExW` (9), `RegQueryValueExW` (6), `RegSetValueExW` (6), `RegEnumKeyExW` (8), `RegEnumValueW` (8), `RegQueryInfoKeyW` (12), `RegDeleteKeyW` (2), `RegDeleteValueW` (2), `RegFlushKey` (1), `RegCloseKey` (1, exists): wrappers over the A registry store; `REG_SZ` values written by the W API are stored as UTF-8 and re-encoded on read (`cbData` counts UTF-16 bytes including the terminator). `RegConnectRegistryW`, `RegLoadKeyW`, `RegUnLoadKeyW`, `RegSaveKeyW`, `RegRestoreKeyW`, `RegReplaceKeyW` return `ERROR_ACCESS_DENIED` (5).
- `version` W: `GetFileVersionInfoSizeW` (2), `GetFileVersionInfoW` (4), `VerQueryValueW` (4): the A bodies over the resource reader (Task 7), with `VerQueryValueW`'s sub-block path (`L"\\"`, `L"\\VarFileInfo\\Translation"`, `L"\\StringFileInfo\\040904E4\\FileVersion"`) parsed from UTF-16.
- `comctl32`: `ImageList_Create` (5) returns a handle to a `std::vector` of 32-bpp images sized cx by cy; `ImageList_Add` (3), `ImageList_Replace` (4), `ImageList_ReplaceIcon` (3), `ImageList_Remove` (2), `ImageList_GetImageCount` (1), `ImageList_SetImageCount` (2), `ImageList_GetIconSize` (3), `ImageList_SetIconSize` (3), `ImageList_GetBkColor` (1), `ImageList_SetBkColor` (2), `ImageList_Draw` (6), `ImageList_DrawEx` (10), `ImageList_GetIcon` (3), `ImageList_GetImageInfo` (3), `ImageList_Copy` (5), `ImageList_Destroy` (1), `ImageList_LoadImageW` (7), `ImageList_Read`/`ImageList_Write` (1, 2: fail, 0), `ImageList_BeginDrag`/`ImageList_EndDrag`/`ImageList_DragEnter`/`ImageList_DragLeave`/`ImageList_DragMove`/`ImageList_DragShowNolock`/`ImageList_GetDragImage`/`ImageList_SetOverlayImage` (accept, return TRUE or NULL as documented), `InitializeFlatSB` (1, TRUE), `FlatSB_*` (6: answer as the plain scroll-bar functions of Task 9 do), `_TrackMouseEvent` (1, TRUE). The VCL creates image lists for its glyph caches even when nothing is drawn with them; drawing (`ImageList_Draw*`) blits into the DC's backing surface from Task 10.
- `winspool.drv`: `EnumPrintersW` (7: 0 printers, `pcReturned = 0`, TRUE), `GetDefaultPrinterW` (2: `ERROR_FILE_NOT_FOUND`, FALSE), `OpenPrinterW` (3, FALSE), `ClosePrinter` (1, TRUE), `DocumentPropertiesW` (6, -1).
- `netapi32`: `NetWkstaGetInfo` (4, returns `ERROR_NOT_SUPPORTED` 50), `NetApiBufferFree` (1, 0).
- `msvcrt`: `memcpy`, `memset` (both cdecl, `ARGC_CDECL`).
- `shfolder`: `SHGetFolderPathW` (5): `CSIDL_PERSONAL` (5), `CSIDL_APPDATA` (26), `CSIDL_LOCAL_APPDATA` (28), `CSIDL_COMMON_APPDATA` (35), `CSIDL_MYDOCUMENTS` (5) all answer the profile's documents path as the guest path the kit's `SHGetSpecialFolderPathA` returns; anything else `E_INVALIDARG`.
- `shell32`: `Shell_NotifyIconW` (2, TRUE).
- `ole32`: `OleInitialize` (1, `S_OK`), `OleUninitialize` (0), `CoInitializeEx` (2, `S_OK`), `CoTaskMemAlloc` (1, guest heap), `CoTaskMemFree` (1), `IsEqualGUID` (2, memcmp 16).

- [ ] **Step 1: Write the failing test**

```cpp
static void test_delphi_dlls() {
    X86 c;
    loader_init_context(&c);
    uint32_t s = 0x00300000;
    // BSTR: length prefix in bytes, text, terminator.
    gm_put_wstr(s, "hello", 16);
    uint32_t b = call_import(&c, "OLEAUT32.dll", "SysAllocStringLen", {s, 5});
    check(b != 0 && rd32(b - 4) == 10 && gm_wstr(b) == "hello", "SysAllocStringLen");
    check(call_import(&c, "OLEAUT32.dll", "SysFreeString", {b}) == 0, "SysFreeString");
    // Registry through the W API, read back through the A one.
    uint32_t key = s + 0x100, hkey_out = s + 0x200;
    gm_put_wstr(key, "Software\\RecompTest", 64);
    check(call_import(&c, "ADVAPI32.dll", "RegCreateKeyExW", {0x80000001u, key, 0, 0, 0, 0xf003f, 0, hkey_out, 0}) == 0,
          "RegCreateKeyExW");
    uint32_t hk = rd32(hkey_out);
    gm_put_wstr(s + 0x300, "Name", 16); gm_put_wstr(s + 0x400, "value", 16);
    check(call_import(&c, "ADVAPI32.dll", "RegSetValueExW", {hk, s + 0x300, 0, 1, s + 0x400, 12}) == 0, "RegSetValueExW");
    gm_put_str(s + 0x500, "Name", 16);
    wr32(s + 0x600, 64);
    check(call_import(&c, "ADVAPI32.dll", "RegQueryValueExA", {hk, s + 0x500, 0, 0, s + 0x700, s + 0x600}) == 0
          && gm_str(s + 0x700) == "value", "RegQueryValueExA reads what RegSetValueExW wrote");
    // version.dll W over the image's own resource.
    gm_put_wstr(s, RECOMP_EXECUTABLE, 128);
    uint32_t size = call_import(&c, "VERSION.dll", "GetFileVersionInfoSizeW", {s, 0});
    check(size > 0, "GetFileVersionInfoSizeW = %u", size);
    check(call_import(&c, "VERSION.dll", "GetFileVersionInfoW", {s, 0, size, s + 0x1000}) == 1, "GetFileVersionInfoW");
    gm_put_wstr(s + 0x800, "\\", 8);
    check(call_import(&c, "VERSION.dll", "VerQueryValueW", {s + 0x1000, s + 0x800, s + 0x900, s + 0x904}) == 1
          && rd32(rd32(s + 0x900)) == 0xfeef04bdu, "VerQueryValueW(\\) finds VS_FIXEDFILEINFO");
    // The rest answer as documented for a machine with nothing attached.
    wr32(s + 0xa00, 0);
    check(call_import(&c, "WINSPOOL.DRV", "EnumPrintersW", {2, 0, 2, 0, 0, s + 0xa04, s + 0xa00}) == 1 && rd32(s + 0xa00) == 0,
          "EnumPrintersW: no printers");
    check(call_import(&c, "NETAPI32.dll", "NetWkstaGetInfo", {0, 100, s + 0xb00}) == 50, "NetWkstaGetInfo: not supported");
    check(call_import(&c, "OLE32.dll", "OleInitialize", {0}) == 0, "OleInitialize");
    uint32_t il = call_import(&c, "COMCTL32.dll", "ImageList_Create", {16, 16, 0x20, 4, 4});
    check(il != 0 && call_import(&c, "COMCTL32.dll", "ImageList_GetImageCount", {il}) == 0, "ImageList_Create");
}
```

- [ ] **Step 2: Run to see it fail**, **Step 3: implement group by group** (each group a file, each file registered from `imports_init`), **Step 4: run** as in Task 7.

- [ ] **Step 5: Format and commit (kit)**

```bash
.venv/bin/python kit/tools/format.py --write
git -C kit add runtime/oleaut32.cpp runtime/comctl32.cpp runtime/misc_dlls.cpp runtime/CMakeLists.txt runtime/tests/runtime_tests.cpp $(git -C kit diff --name-only runtime dx)
git -C kit commit -m "Runtime: oleaut32, comctl32, the wide registry and version APIs, and the DLLs that answer 'nothing attached'"
```

### Task 9: The wide user32 layer and the VCL's window model

**Files:**
- Create: `kit/runtime/user32_wide.cpp` (W wrappers), `kit/runtime/user32_vcl.cpp` (what the VCL needs that no game asked for before: timers, properties, menus, monitors, hooks, clipboard formats, accelerators)
- Modify: `kit/runtime/user32.cpp` (factor `register_class_named`, `create_window_named`, `def_window_proc`, `send_message`, `peek_message`, `dispatch_message`; give `struct Window` (line 32) a `bool unicode`, a `std::map<std::string, uint32_t> props`, a `uint32_t menu`, and a `std::string title_utf8`; give `struct Msg` a `uint32_t time`)
- Test: `kit/runtime/tests/runtime_tests.cpp` (`test_user32_vcl`)

**Interfaces:**
- Wrappers (same body as the A shim, text through `gm_wstr`/`gm_put_wstr`, and the message queue delivers UTF-16 for `WM_SETTEXT` (0x0c), `WM_GETTEXT` (0x0d), `WM_CHAR` (0x102) to a window created by the W API): `RegisterClassW` (1), `UnregisterClassW` (2), `GetClassInfoW` (3), `CreateWindowExW` (12), `DefWindowProcW` (4), `DefFrameProcW` (5), `DefMDIChildProcW` (4), `CallWindowProcW` (5), `PeekMessageW` (5), `DispatchMessageW` (1), `SendMessageW` (4), `SendMessageTimeoutW` (7), `PostMessageW` (4), `SetWindowLongW` (3), `GetWindowLongW` (2), `GetClassLongW` (2), `SetClassLongW` (3), `GetWindowTextW` (3), `SetWindowTextW` (2), `GetClassNameW` (3), `FindWindowW` (2), `FindWindowExW` (4), `LoadCursorW` (2), `LoadIconW` (2), `LoadBitmapW` (2), `LoadStringW` (4, over the resource reader: string table block `(id >> 4) + 1`, entry `id & 15`), `DrawTextW` (5), `DrawTextExW` (6), `CharUpperW`, `CharLowerW`, `CharUpperBuffW`, `CharLowerBuffW`, `CharNextW`, `MapVirtualKeyW` (2), `GetKeyNameTextW` (3), `GetKeyboardLayoutNameW` (1), `LoadKeyboardLayoutW` (2), `GetMenuStringW`, `GetMenuItemInfoW`, `SetMenuItemInfoW`, `InsertMenuW`, `InsertMenuItemW`, `MessageBoxW` (exists), `RegisterClipboardFormatW` (1), `RegisterWindowMessageW` (1, `0xc000` upwards), `SystemParametersInfoW` (4), `EnumDisplaySettingsW` (3), `EnumDisplayDevicesW` (4), `GetMonitorInfoW` (2), `IsDialogMessageW` (2), `CreateAcceleratorTableW` (2), `SetWindowsHookExW` (4).
- The VCL model (`user32_vcl.cpp`): `SetTimer` (4) and `KillTimer` (2): a timer list the message pump drains, posting `WM_TIMER` (0x113) when `host_millis()` passes the deadline, callback timers called through `recomp_call`; `GetPropW`/`SetPropW`/`RemovePropW` on `Window::props`; `SetParent`, `GetParent`, `IsChild`, `EnumChildWindows`, `EnumThreadWindows`, `EnumWindows`, `GetTopWindow`, `GetWindow` (2), `GetWindowThreadProcessId`, `IsWindow`, `IsWindowVisible`, `IsWindowEnabled`, `EnableWindow`, `IsZoomed`, `IsIconic`, `GetWindowPlacement`, `SetWindowPlacement`, `SetForegroundWindow`, `SetActiveWindow`, `GetFocus`, `GetCapture`, `SetCapture`, `ReleaseCapture`, `WindowFromPoint`, `MonitorFromWindow`/`MonitorFromPoint`/`MonitorFromRect` (one monitor handle `1`), `EnumDisplayMonitors` (callback once through `recomp_call` with the display rectangle), `GetDesktopWindow` (a fixed handle whose rectangle is the display), `GetDCEx`, `GetWindowDC`, `RedrawWindow`, `SetWindowRgn`, `ScrollWindow`, `MapWindowPoints`, `IntersectRect`, `FillRect`, `FrameRect`, `DrawEdge`, `DrawFrameControl`, `DrawFocusRect`, `DrawIcon`, `DrawIconEx`, `CopyIcon`, `CopyImage`, `CreateIcon`, `DestroyCursor`, `GetIconInfo`, `GetCursor`, `GetMessagePos`, `GetMessageExtraInfo`, `GetKeyboardState` (256 bytes from the input gate), `GetKeyboardLayoutList`, `ActivateKeyboardLayout`, `MsgWaitForMultipleObjects` (5) and `MsgWaitForMultipleObjectsEx` (5): return `WAIT_OBJECT_0 + count` when a message is queued else run the host's frame pump once and return `WAIT_TIMEOUT` (0x102), `WaitMessage` (0: pump once), menus (`CreateMenu`, `CreatePopupMenu`, `DestroyMenu`, `GetMenu`, `SetMenu`, `GetSystemMenu`, `GetSubMenu`, `GetMenuItemCount`, `GetMenuItemID`, `GetMenuState`, `CheckMenuItem`, `EnableMenuItem`, `RemoveMenu`, `DeleteMenu`, `DrawMenuBar`, `TrackPopupMenu` (returns 0), `EndMenu`) over a `std::map<uint32_t, Menu>`; scroll bars (`GetScrollPos`, `SetScrollPos`, `GetScrollRange`, `SetScrollRange`, `GetScrollInfo`, `SetScrollInfo`, `ShowScrollBar`, `EnableScrollBar`) over per-window `ScrollInfo` records; clipboard (`EmptyClipboard`, `SetClipboardData` in addition to the existing three); hooks (`SetWindowsHookExW` returns a handle, `UnhookWindowsHookEx`, `CallNextHookEx` returns 0); `GetSysColor` (30 entries of the classic palette), `GetSysColorBrush`; `GetSystemMetrics` gaps (`SM_CXSCREEN`..`SM_CYSMICON`, answer for the display size the kit presents); `ShowOwnedPopups`, `GetLastActivePopup`, `MessageBeep`, `PostQuitMessage` (exists), `GetDlgCtrlID`, `TranslateMDISysAccel` (0), `ShowCaret`/`HideCaret` (TRUE), `ClipCursor` (exists).
- `IsWindowUnicode` returns `Window::unicode`.

- [ ] **Step 1: Write the failing test**

```cpp
static uint32_t g_wndproc_calls;
static void test_user32_vcl() {
    X86 c;
    loader_init_context(&c);
    uint32_t s = 0x00300000;
    // A WNDCLASSW whose procedure is DefWindowProcW's trampoline: every message
    // the pump delivers comes back through the runtime, which is enough to
    // prove creation, text and timers.
    uint32_t defproc = imports_resolve("USER32.dll", "DefWindowProcW");
    uint32_t wc = s; gm_zero(wc, 40);
    wr32(wc + 4, defproc);
    gm_put_wstr(s + 0x100, "TVclTestWindow", 32);
    wr32(wc + 36, s + 0x100);
    check(call_import(&c, "USER32.dll", "RegisterClassW", {wc}) != 0, "RegisterClassW");
    gm_put_wstr(s + 0x200, "Siege", 16);
    uint32_t hwnd = call_import(&c, "USER32.dll", "CreateWindowExW",
                                {0, s + 0x100, s + 0x200, 0x00cf0000u, 0, 0, 800, 600, 0, 0, IMAGE_BASE, 0});
    check(hwnd != 0, "CreateWindowExW -> %08x", hwnd);
    check(call_import(&c, "USER32.dll", "IsWindowUnicode", {hwnd}) == 1, "a W window is Unicode");
    check(call_import(&c, "USER32.dll", "GetWindowTextW", {hwnd, s + 0x300, 32}) == 5 && gm_wstr(s + 0x300) == "Siege",
          "GetWindowTextW");
    gm_put_wstr(s + 0x400, "prop", 8);
    check(call_import(&c, "USER32.dll", "SetPropW", {hwnd, s + 0x400, 0x1234}) == 1
          && call_import(&c, "USER32.dll", "GetPropW", {hwnd, s + 0x400}) == 0x1234, "window properties");
    check(call_import(&c, "USER32.dll", "SetTimer", {hwnd, 7, 10, 0}) == 7, "SetTimer");
    host_advance_millis(20); // the test clock helper the frame-clock tests use; add one if absent
    uint32_t msg = s + 0x500;
    check(call_import(&c, "USER32.dll", "PeekMessageW", {msg, 0, 0, 0, 1}) == 1 && rd32(msg + 4) == 0x113 && rd32(msg + 8) == 7,
          "WM_TIMER 7 is queued after the deadline");
    check(call_import(&c, "USER32.dll", "KillTimer", {hwnd, 7}) == 1, "KillTimer");
    check(call_import(&c, "USER32.dll", "GetSysColor", {15}) == 0x00f0f0f0u, "COLOR_BTNFACE");
    check(call_import(&c, "USER32.dll", "DestroyWindow", {hwnd}) == 1, "DestroyWindow");
}
```

- [ ] **Step 2: Run to see it fail**, **Step 3: implement** (wrappers first, then the model, running the test after each), **Step 4: run** as before; also run `tools/test.py --native -R host_tests` because `user32.cpp` is linked into the hosts.

- [ ] **Step 5: Format and commit (kit)**

```bash
.venv/bin/python kit/tools/format.py --write
git -C kit add runtime/user32.cpp runtime/user32_wide.cpp runtime/user32_vcl.cpp runtime/CMakeLists.txt runtime/tests/runtime_tests.cpp
git -C kit commit -m "Runtime: the wide user32 API and the window model a VCL application drives"
```

### Task 10: GDI for a VCL canvas: window surfaces, DIBs, text, pens, brushes, regions

**Files:**
- Modify: `kit/runtime/gdi32.cpp` (the 23 shims today; add the DC model)
- Create: `kit/runtime/gdi32_draw.cpp` (raster operations on 32-bpp backing surfaces), `kit/runtime/gdi32_text.cpp` (font objects and text through the kit's bitmap font), `kit/runtime/gdi32_font8x16.h` (the glyphs the page overlay already draws with, if `host/page_overlay.cpp` has a table: reuse it by moving it to a shared header; else a 96-glyph 8x16 table)
- Modify: `kit/runtime/display_seam.h` and its implementation (the seam `g_BitBlt` reaches the host through) so a window surface can be presented when no DirectDraw primary exists
- Test: `kit/runtime/tests/gdi_tests.cpp` (new CTest entry `gdi_tests`, label `nogame`, next to `runtime_tests` in `runtime/CMakeLists.txt`)

**Interfaces:**
- Model: every `HDC` names a `DeviceContext { uint32_t surface; int32_t org_x, org_y; uint32_t pen, brush, font, bitmap, palette, region; uint32_t text_color, bk_color; int bk_mode, rop2, stretch_mode; int32_t pos_x, pos_y; std::vector<DcState> saved; }`; every top-level window from Task 9 owns a `Surface { int w, h; std::vector<uint32_t> argb; }` that `GetDC`/`BeginPaint` select and that the seam presents on `EndPaint`/`ReleaseDC` when the window is visible and no DirectDraw primary is active; `CreateCompatibleDC` selects no surface until `SelectObject` gives it a bitmap; a DIB section's bits stay in guest memory (existing `g_CreateDIBSection`) and the DC reads and writes them in place.
- New shims (arity): `CreateSolidBrush` (1), `CreateBrushIndirect` (1), `CreatePenIndirect` (1), `CreateFontIndirectW` (1), `GetObjectW` (3), `CreateBitmap` (5), `CreateDIBitmap` (6), `SetDIBits` (7), `SetDIBitsToDevice` (12), `StretchDIBits` (13), `GetDIBColorTable` (4), `GetBitmapBits` (3), `StretchBlt` (11), `MaskBlt` (12, as `BitBlt` ignoring the mask when it is 0), `SetStretchBltMode`/`GetStretchBltMode`, `SaveDC`/`RestoreDC`, `SetBkColor`, `GetDeviceCaps` (2: `HORZRES` 8/`VERTRES` 10 the display, `BITSPIXEL` 12 = 32, `PLANES` 14 = 1, `LOGPIXELSX/Y` 88/90 = 96, `RASTERCAPS` 38 = `RC_BITBLT|RC_DIBTODEV|RC_STRETCHBLT|RC_STRETCHDIB`, `NUMCOLORS` 24 = -1, `TECHNOLOGY` 2 = 1), `GetPixel`, `SetPixel`, `MoveToEx`, `LineTo`, `Polyline`, `Polygon`, `Rectangle`, `RoundRect` (as `Rectangle`), `Ellipse`, `Arc`/`ArcTo`/`AngleArc`/`Chord`/`Pie`/`PolyBezier`/`PolyBezierTo` (draw the polyline through the control points), `ExtFloodFill` (4-connected fill), `CreateRectRgn`, `SetRectRgn`, `GetRgnBox`, `FrameRgn`, `IntersectClipRect`, `ExcludeClipRect`, `GetClipBox`, `RectVisible`, `SetROP2`, `SetWindowOrgEx`/`GetWindowOrgEx`/`SetViewportOrgEx`, `SetBrushOrgEx`/`GetBrushOrgEx`, `GetCurrentPositionEx`, `UnrealizeObject`, `ResizePalette`, `CreateHalftonePalette`, `GetNearestPaletteIndex`, `GdiFlush` (TRUE), `AddFontMemResourceEx` (returns a handle, the font is the built-in), `EnumFontsW`/`EnumFontFamiliesExW` (callback once through `recomp_call` with a `LOGFONTW` for "recomp" and `TEXTMETRICW` 8x16), text: `ExtTextOutW` (8), `GetTextExtentPoint32W` (4), `GetTextExtentPointW` (4), `GetTextMetricsW` (2); metafiles and printing (`Create/DeleteEnhMetaFile`, `GetEnhMetaFile*`, `SetEnhMetaFileBits`, `SetWinMetaFileBits`, `GetWinMetaFileBits`, `PlayEnhMetaFile`, `CopyEnhMetaFileW`, `CreateDCW`, `CreateICW`, `StartDocW`, `EndDoc`, `StartPage`, `EndPage`, `AbortDoc`, `SetAbortProc`): return 0.
- Text: `CreateFontIndirectW` records the `LOGFONTW` height and weight; `ExtTextOutW` draws with the 8x16 glyph table scaled to `max(1, |lfHeight| / 16)` in the DC's text colour, opaque background when `bk_mode == OPAQUE` (2); `GetTextExtentPoint32W` returns `8 * scale * n` by `16 * scale`; `GetTextMetricsW` fills `tmHeight 16*scale`, `tmAscent 13*scale`, `tmDescent 3*scale`, `tmAveCharWidth 8*scale`, `tmMaxCharWidth 8*scale`, `tmCharSet 0`, `tmPitchAndFamily 0x30` (fixed pitch, modern). Host font rendering (CoreText, DirectWrite, FreeType) is a later polish item and is not in this plan.

- [ ] **Step 1: Write the failing tests**

`gdi_tests.cpp` (its own `main`, the `check` helper copied from `runtime_tests.cpp`):

```cpp
static void test_window_surface_and_blits() {
    X86 c; loader_init_context(&c);
    uint32_t s = 0x00300000;
    uint32_t hwnd = make_test_window(&c, s, 64, 48);   // RegisterClassW + CreateWindowExW as in test_user32_vcl
    uint32_t dc = call_import(&c, "USER32.dll", "GetDC", {hwnd});
    check(dc != 0, "GetDC");
    uint32_t brush = call_import(&c, "GDI32.dll", "CreateSolidBrush", {0x00ff0000u}); // COLORREF 0x00bbggrr: blue
    uint32_t rect = s + 0x100; wr32(rect, 4); wr32(rect + 4, 4); wr32(rect + 8, 20); wr32(rect + 12, 20);
    check(call_import(&c, "USER32.dll", "FillRect", {dc, rect, brush}) != 0, "FillRect");
    check(call_import(&c, "GDI32.dll", "GetPixel", {dc, 5, 5}) == 0x00ff0000u, "the fill is readable");
    check(call_import(&c, "GDI32.dll", "GetPixel", {dc, 30, 30}) == 0x00000000u, "outside the rectangle is untouched");
    // A memory DC with a DIB section, blitted onto the window with a stretch.
    uint32_t mem = call_import(&c, "GDI32.dll", "CreateCompatibleDC", {dc});
    uint32_t bmi = s + 0x200; gm_zero(bmi, 40);
    wr32(bmi, 40); wr32(bmi + 4, 8); wr32(bmi + 8, 8); wr16(bmi + 12, 1); wr16(bmi + 14, 32);
    uint32_t bits_out = s + 0x300;
    uint32_t dib = call_import(&c, "GDI32.dll", "CreateDIBSection", {mem, bmi, 0, bits_out, 0, 0});
    check(dib != 0, "CreateDIBSection");
    uint32_t bits = rd32(bits_out);
    for (int i = 0; i < 64; ++i) wr32(bits + 4 * i, 0x0000ff00u);
    call_import(&c, "GDI32.dll", "SelectObject", {mem, dib});
    check(call_import(&c, "GDI32.dll", "StretchBlt", {dc, 32, 0, 16, 16, mem, 0, 0, 8, 8, 0x00cc0020u}) == 1, "StretchBlt");
    check(call_import(&c, "GDI32.dll", "GetPixel", {dc, 47, 15}) == 0x0000ff00u, "the stretched blit reached the corner");
    // Text: a glyph is drawn in the text colour and measured at 8x16.
    call_import(&c, "GDI32.dll", "SetTextColor", {dc, 0x000000ffu});
    gm_put_wstr(s + 0x400, "A", 4);
    check(call_import(&c, "GDI32.dll", "ExtTextOutW", {dc, 0, 32, 0, 0, s + 0x400, 1, 0}) == 1, "ExtTextOutW");
    uint32_t sz = s + 0x500;
    check(call_import(&c, "GDI32.dll", "GetTextExtentPoint32W", {dc, s + 0x400, 1, sz}) == 1 && rd32(sz) == 8 && rd32(sz + 4) == 16,
          "GetTextExtentPoint32W = %ux%u", rd32(sz), rd32(sz + 4));
    bool any_red = false;
    for (int y = 32; y < 48 && !any_red; ++y)
        for (int x = 0; x < 8; ++x)
            if (call_import(&c, "GDI32.dll", "GetPixel", {dc, (uint32_t)x, (uint32_t)y}) == 0x000000ffu) { any_red = true; break; }
    check(any_red, "the glyph left red pixels");
    call_import(&c, "USER32.dll", "ReleaseDC", {hwnd, dc});
}
```

- [ ] **Step 2: Add the CTest target and run to see it fail**

In `runtime/CMakeLists.txt`, copy the `runtime_tests` `add_executable`/`add_test` block for `gdi_tests` with label `nogame`. Run: `.venv/bin/python tools/test.py --native -R gdi_tests > build/gdi_tests.log 2>&1; tail -5 build/gdi_tests.log`. Expected: `no trampoline for GDI32.dll!CreateSolidBrush`.

- [ ] **Step 3: Implement the model, then the blits, then text**, running `gdi_tests` after each.

- [ ] **Step 4: Run every native suite**

Run: `.venv/bin/python tools/test.py --native > build/native.log 2>&1; grep -E "tests passed|failed" build/native.log | tail -3`
Expected: `gdi_tests` and `runtime_tests` pass; `dx_tests` unchanged (its `g_BitBlt` onto a DirectDraw primary still works: the surface model must keep the primary as the blit target when one exists).

- [ ] **Step 5: Format and commit (kit)**

```bash
.venv/bin/python kit/tools/format.py --write
git -C kit add runtime/gdi32.cpp runtime/gdi32_draw.cpp runtime/gdi32_text.cpp runtime/gdi32_font8x16.h runtime/display_seam.h runtime/CMakeLists.txt runtime/tests/gdi_tests.cpp $(git -C kit diff --name-only host runtime)
git -C kit commit -m "GDI: window surfaces, DIB blits, pens, brushes, regions and bitmap-font text for a VCL canvas"
```

### Task 11: FMOD 3 as a shim module, plus the MIDI helper and the Galaxy stub

**Files:**
- Create: `kit/dx/fmod.cpp` (18 `FSOUND_*` exports over the kit mixer and minimp3), `kit/dx/soundlib.cpp` (the five `*Midi` exports over `host/midi.h`), `kit/dx/galaxy_stub.cpp` (`cgGetGalaxyAPI` → 0)
- Modify: `kit/dx/CMakeLists.txt`, `kit/dx/dx.cpp` (call the three modules' `register` functions where `qmixer` and `weanetr` register)
- Test: `kit/dx/tests/dx_tests.cpp` (`test_fmod`, `test_soundlib_stub`, in the shape of `test_qmixer` at line 5433)

**Interfaces:**
- Names are the decorated stdcall names the import table carries: `_FSOUND_Init@12`, `_FSOUND_Close@0`, `_FSOUND_SetOutput@4`, `_FSOUND_SetDriver@4`, `_FSOUND_GetDriverName@4`, `_FSOUND_Sample_LoadWav@12`, `_FSOUND_Sample_Free@4`, `_FSOUND_Sample_GetDefaults@20`, `_FSOUND_Sample_SetLoopMode@8`, `_FSOUND_PlaySoundAttrib@20`, `_FSOUND_StopSound@4`, `_FSOUND_SetVolume@8`, `_FSOUND_SetPan@8`, `_FSOUND_SetFrequency@8`, `_FSOUND_Stream_OpenMpeg@8`, `_FSOUND_Stream_Play@8`, `_FSOUND_Stream_SetPaused@8`, `_FSOUND_Stream_Close@4`; the `@N` gives `argc_stdcall = N / 4`. Register under DLL name `"fmod.dll"` (the import table's spelling, lower-case).
- FMOD 3.20 semantics: `FSOUND_Init(mixrate, maxchannels, flags)` → 1; `FSOUND_SetOutput(type)`/`FSOUND_SetDriver(n)` → 1; `FSOUND_GetDriverName(n)` → pointer to a static guest string `"recomp mixer"`; `FSOUND_Sample_LoadWav(index, name_or_data, mode)`: `index` is a slot (`FSOUND_FREE` = -1 means any), `mode` bit `FSOUND_LOADMEMORY` (0x8000) means `name_or_data` points at the RIFF bytes in guest memory, else it is a guest path; returns a sample handle (non-zero) or 0; decode with the RIFF reader `qmixer.cpp` uses (`dxtypes.h` WAVEFORMATEX) and keep the PCM in a `Sample { rate, channels, bits, pcm, loop }`; `FSOUND_Sample_GetDefaults(sample, &freq, &vol, &pan, &pri)` writes the sample's defaults where the pointers are non-null; `FSOUND_Sample_SetLoopMode(sample, mode)` (`FSOUND_LOOP_NORMAL` 2); `FSOUND_PlaySoundAttrib(channel, sample, freq, vol, pan)` → the channel number the mixer voice got (0..maxchannels-1) or -1; `FSOUND_StopSound(channel)`, `FSOUND_SetVolume(channel, 0..255)`, `FSOUND_SetPan(channel, 0..255, 128 centre)`, `FSOUND_SetFrequency(channel, hz)` → 1; `FSOUND_Stream_OpenMpeg(name, mode)` → stream handle: an MP3 decoded on demand through the minimp3 path `dx/dshow.cpp` uses (factor its decoder into `dx/mp3_source.h` so both use it), looping when `mode` has `FSOUND_LOOP_NORMAL`; `FSOUND_Stream_Play(channel, stream)` → channel; `FSOUND_Stream_SetPaused(stream, paused)` → 1; `FSOUND_Stream_Close(stream)` → 1; `FSOUND_Close()`.
- `Soundlib.dll`: arities are read off the DLL before implementing: `python3 -c` with capstone over each export's body in `original/gog/Soundlib.dll` to find its `RET imm16` (record the five values in the file's header comment as `qmixer.cpp` does). Implementation: `CreateMidi` → handle, `OpenMidi(handle, path)` loads a `.mid` from the guest path into `host/midi.h`'s player, `PlayMidi`/`StopMidi`, `SetMidiVolume(handle, 0..100)`, `GetMidiVolume`, `FreeMidi`.
- `CGalaxy.dll`: `cgGetGalaxyAPI` (0 arguments) → 0; the game's Galaxy wrapper treats a null API as "offline".

- [ ] **Step 1: Write the failing test**

```cpp
static void test_fmod() {
    cpu_reset();
    uint32_t init = tramp("fmod.dll", "_FSOUND_Init@12");
    CHECK(init != 0);
    CHECK_EQ(call_shim(init, {44100, 32, 0}), 1u);
    // An 8-frame mono 16-bit RIFF in guest memory, loaded through FSOUND_LOADMEMORY.
    uint32_t wav = sc(0x100);
    write_test_riff(wav, /*rate*/ 22050, /*channels*/ 1, /*bits*/ 16, /*frames*/ 8); // helper: the RIFF the qmixer tests build
    uint32_t load = tramp("fmod.dll", "_FSOUND_Sample_LoadWav@12");
    uint32_t sample = call_shim(load, {0xffffffffu, wav, 0x8000});
    CHECK(sample != 0);
    uint32_t defaults = sc(0x200);
    CHECK_EQ(call_shim(tramp("fmod.dll", "_FSOUND_Sample_GetDefaults@20"), {sample, defaults, defaults + 4, defaults + 8, defaults + 12}), 1u);
    CHECK_EQ(rd32(defaults), 22050u);
    uint32_t play = tramp("fmod.dll", "_FSOUND_PlaySoundAttrib@20");
    uint32_t ch = call_shim(play, {0xffffffffu, sample, 22050, 200, 128});
    CHECK(ch != 0xffffffffu);
    CHECK_EQ(g_plays.size(), 1u);                       // the mixer saw one voice start
    CHECK_EQ(call_shim(tramp("fmod.dll", "_FSOUND_SetVolume@8"), {ch, 64}), 1u);
    CHECK_EQ(call_shim(tramp("fmod.dll", "_FSOUND_StopSound@4"), {ch}), 1u);
    CHECK_EQ(call_shim(tramp("fmod.dll", "_FSOUND_Sample_Free@4"), {sample}), 0u);
    CHECK_EQ(call_shim(tramp("fmod.dll", "_FSOUND_Close@0"), {}), 0u);
    CHECK_EQ(call_shim(tramp("CGalaxy.dll", "cgGetGalaxyAPI"), {}), 0u);
}
```

- [ ] **Step 2: Run to see it fail**: `.venv/bin/python tools/test.py --native -R dx_tests > build/dx_tests.log 2>&1; grep -E "FAIL|no trampoline" build/dx_tests.log | head -3`. Expected: `no trampoline for fmod.dll!_FSOUND_Init@12`.

- [ ] **Step 3: Implement** `fmod.cpp` over the mixer API `qmixer.cpp` calls (read `host/audio/mixer.cpp`'s public functions first: voice start, stop, gain, pan, rate), then `soundlib.cpp` and `galaxy_stub.cpp`.

- [ ] **Step 4: Run** `dx_tests`; expected all green including the existing `test_qmixer`.

- [ ] **Step 5: Format and commit (kit)**

```bash
.venv/bin/python kit/tools/format.py --write
git -C kit add dx/fmod.cpp dx/soundlib.cpp dx/galaxy_stub.cpp dx/mp3_source.h dx/dshow.cpp dx/dx.cpp dx/CMakeLists.txt dx/tests/dx_tests.cpp
git -C kit commit -m "dx: FMOD 3 sound and MP3 streams over the mixer, the MIDI helper, and an offline Galaxy"
```

### Task 12: Structured exception handling for Delphi frames (design, translator, runtime)

**Files:**
- Create: `kit/docs/superpowers/specs/2026-09-14-seh-design.md`
- Modify: `kit/tools/recomp/translate.py` (recognise the frame idiom; emit the host `setjmp` form and a landing dispatch)
- Create: `kit/runtime/seh.cpp`, `kit/runtime/seh.h`
- Modify: `kit/runtime/kernel32.cpp` (`k_RaiseException` line 3441, `k_RtlUnwind` line 3472 call into `seh.cpp` instead of aborting), `kit/runtime/x86.h` (the `CALL_FN` contract, if the unwind needs a post-call check)
- Test: `kit/tools/recomp/tests/test_translate_seh.py` (new), `kit/runtime/tests/seh_tests.cpp` (new CTest entry, label `nogame`)

**Interfaces:**
- The Delphi frame idiom, measured in this game's listings (2,258 frames in 1,728 functions): `XOR EAX,EAX` … `PUSH EBP` / `PUSH <handler>` / `PUSH dword ptr FS:[EAX]` / `MOV dword ptr FS:[EAX],ESP` establishes; `MOV dword ptr FS:[EAX],EDX` (or `ECX`) after popping restores (2,679 sites). `<handler>` is an address inside the same function: a `JMP` to one of three routines in the Delphi `System` unit (`@HandleAnyException`, `@HandleOnException`, `@HandleFinally`), followed by the except or finally block. The three routines are found as the `JMP` targets shared by every handler stub; record their addresses in the spec (they are game-specific and go in the spec's measurement table, not in kit code).
- Windows' contract the runtime implements: `RaiseException(code, flags, nargs, args)` builds an `EXCEPTION_RECORD` (`ExceptionCode`, `ExceptionFlags`, `ExceptionRecord = 0`, `ExceptionAddress = return address`, `NumberParameters`, `ExceptionInformation[15]`) and a `CONTEXT` (the x86 record: EDI, ESI, EBX, EDX, ECX, EAX, EBP, EIP, CS, EFlags, ESP, SS at their documented offsets 0x9c-0xc8) in guest memory, then walks the chain from `FS:[0]`: for each `EXCEPTION_REGISTRATION { next, handler }` it calls `handler(record, registration, context, dispatcher)` through `recomp_call` (cdecl, four arguments, return sentinel) and reads EAX: `ExceptionContinueExecution` (0) resumes from the context, `ExceptionContinueSearch` (1) moves to `next`; a handler that handles does not return: Delphi's routines call `RtlUnwind(target_frame, target_ip, record, retval)` and then transfer control themselves. `RtlUnwind` walks from `FS:[0]` to `target_frame`, calling each handler with `EXCEPTION_UNWINDING` (2) set in `ExceptionFlags`, unlinks them (`FS:[0] = target_frame`), and returns to its caller (Delphi passes `target_ip` = its own return address, so a normal shim return is the transfer).
- The host-stack problem and its solution: after the handler has run the except block it continues *the establishing function* at an interior address while the host stack still holds every C frame between that function and the `RaiseException` shim. The translator therefore emits, at each `MOV dword ptr FS:[EAX],ESP` (frame establishment), the two-call host `setjmp` form the `_setjmp` intrinsic already uses (`runtime/cpu.cpp:178`): `{ jmp_buf *b = recomp_seh_frame_enter(c); if (setjmp(*b)) goto seh_landing_<fn>; }`, and at the top of every function that establishes frames a landing dispatch `seh_landing_<fn>: switch (c->eip) { case 0x<handler>: goto L_<handler>; case 0x<handler>+5: goto L_<handler+5>; ... }` over that function's handler stubs and the instruction after each stub's `JMP` (the block Delphi's routines jump to). The runtime keeps `SehFrame { uint32_t esp; jmp_buf env; }` records keyed by the frame's guest ESP; `recomp_seh_frame_enter` pushes one and `MOV FS:[EAX],EDX` (frame restore) pops it (the translator emits `recomp_seh_frame_leave(c)` there). When a Delphi handler routine, after `RtlUnwind`, performs its `JMP` to the except block (an indirect or direct jump to an interior address of another function), the emitted `recomp_jump` reaches `seh_resume(c, target)`: it finds the innermost `SehFrame` whose `esp` equals the frame the handler restored into ESP (Delphi sets `ESP` to the registration record before the jump), sets `c->eip = target`, and `longjmp`s to its `env`, discarding every host frame above the establishing function. The setjmp form is only emitted for functions that contain the idiom, so the 7,000 other functions pay nothing.
- Unit-level proof without the game: `test_translate_seh.py` translates a synthetic listing with one frame and asserts the emitted C contains `recomp_seh_frame_enter`, `seh_landing_`, and a `case` for the handler address and for `handler + 5`; `seh_tests.cpp` builds a chain by hand in guest memory with a handler whose trampoline is a host function registered through `imports_alloc_trampoline`, calls `RaiseException` through `call_import`, and checks the handler saw the record and that `RtlUnwind` unlinked to the target frame and returned.

- [ ] **Step 1: Write the spec**

`kit/docs/superpowers/specs/2026-09-14-seh-design.md` with sections: Goal; the Delphi idiom (the listing excerpt from `0x00881137`-`0x00881143` in this game and the counts above); Windows' contract (the two structures with offsets); the host-stack problem; the setjmp-and-landing design; what is emitted per function; runtime records; failure modes (a `longjmp` across a frame that holds a host lock: forbid the shims that take locks from calling guest code, which the kit's threading section already requires; a raise with no frame: log the record as `k_RaiseException` does today and abort); testing; out of scope (C++ `__CxxThrowException` frames of MSVC games keep aborting as today, with a note that the same machinery could serve them).

- [ ] **Step 2: Write the failing translator test**

`kit/tools/recomp/tests/test_translate_seh.py`:

```python
"""A Delphi exception frame becomes a host setjmp and a landing dispatch."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(HERE))))
from tests.test_translate_insns import translate_case_to_c, Case   # the helper the insn suite uses to get C text

FRAME = Case("try/finally frame", 0x0D030000,
             [(0x00, "XOR EAX,EAX"), (0x02, "PUSH EBP"), (0x03, "PUSH 0x0d030020"),
              (0x08, "PUSH dword ptr FS:[EAX]"), (0x0b, "MOV dword ptr FS:[EAX],ESP"),
              (0x0e, "MOV EAX,0x1"), (0x13, "XOR EAX,EAX"), (0x15, "POP EDX"), (0x16, "POP ECX"),
              (0x17, "POP ECX"), (0x18, "MOV dword ptr FS:[EAX],EDX"), (0x1b, "JMP 0x0d030027"),
              (0x20, "JMP 0x0d031000"), (0x25, "MOV EAX,0x2"), (0x2a, "RET"),
              (0x27, "RET")],
             "31 C0 55 68 20 00 03 0D 64 FF 30 64 89 20 B8 01 00 00 00 31 C0 5A 59 59 64 89 15 EB 0A "
             "E9 DB 0F 00 00 B8 02 00 00 00 C3 C3")

def test_frame_emits_setjmp_and_landing():
    c = translate_case_to_c(FRAME)
    assert "recomp_seh_frame_enter(c)" in c
    assert "recomp_seh_frame_leave(c)" in c
    assert "seh_landing_0d030000:" in c
    assert "case 0x0d030020u:" in c and "case 0x0d030025u:" in c
```

(Adjust the byte string so the offsets in `lines` match; assemble with `python3 -c "import keystone"` if available, else by hand as above and verify with `capstone`.)

- [ ] **Step 3: Run it to see it fail**: `.venv/bin/python -m pytest -q tools/recomp/tests/test_translate_seh.py 2>&1 | tail -3`. Expected: assertion on `recomp_seh_frame_enter`.

- [ ] **Step 4: Implement the translator side**

In `Translator.prepare(fn)` collect `fn.seh_handlers`: for each `MOV dword ptr FS:[<reg>],ESP` at index `i`, the nearest preceding `PUSH <imm>` whose immediate lies in `fn.addrs`; record `(imm, imm + len(JMP at imm))`. In `_emit`, for `MOV dword ptr FS:[...],ESP` emit the two-call form:

```python
        if m == "MOV" and ops[0].kind == "mem" and ops[0].seg == "FS" and ops[1].kind == "reg" and ops[1].reg == 4:
            return [write_op(ops[0], 32, "c->r[4]"),
                    "{ jmp_buf *b_ = recomp_seh_frame_enter(c); if (setjmp(*b_)) goto seh_landing_%08x; }" % fn.addr]
```

and for `MOV dword ptr FS:[...],<EDX|ECX>` append `recomp_seh_frame_leave(c);` after the store. Where the function body is assembled (the place that writes `L_<addr>: ;` labels), emit after the prologue:

```python
        if fn.seh_handlers:
            body.append("if (0) { seh_landing_%08x: switch (c->eip) {" % fn.addr)
            for stub, after in sorted(fn.seh_handlers):
                body.append("case 0x%08xu: goto L_%08x; case 0x%08xu: goto L_%08x;" % (stub, stub, after, after))
            body.append("default: recomp_seh_bad_landing(c); return; } }")
```

Make sure the labels `L_<stub>` and `L_<after>` exist (they are instruction boundaries the emitter already labels when they are branch targets; force them for these addresses).

- [ ] **Step 5: Write the failing runtime test**

`kit/runtime/tests/seh_tests.cpp`:

```cpp
// seh_tests.cpp - RaiseException walks the guest's FS:[0] chain; RtlUnwind unlinks to a frame.
static uint32_t g_seen_code, g_seen_flags, g_handler_calls;
static void host_handler(X86 *c) {            // EXCEPTION_DISPOSITION handler(record, frame, context, dispatcher)
    uint32_t record = rd32(c->r[4] + 4);
    g_seen_code = rd32(record + 0); g_seen_flags = rd32(record + 4); ++g_handler_calls;
    c->r[0] = 1;                              // ExceptionContinueSearch
}
static void test_chain_walk_and_unwind() {
    X86 c; loader_init_context(&c);
    uint32_t handler = imports_alloc_trampoline("TEST.dll", "handler", host_handler, ARGC_CDECL);
    uint32_t outer = 0x00300100, inner = 0x00300200;
    wr32(outer, 0xffffffffu); wr32(outer + 4, handler);
    wr32(inner, outer);       wr32(inner + 4, handler);
    wr32(c.fs_base, inner);
    g_handler_calls = 0;
    // A raise no handler accepts: both are asked, then the runtime reports and stops the guest
    // (recomp_seh_unhandled), which the test intercepts through the hook seh.h exposes for tests.
    seh_set_unhandled_hook([](X86 *) {});
    call_import(&c, "KERNEL32.dll", "RaiseException", {0x0eedfade, 1, 0, 0});
    check(g_handler_calls == 2 && g_seen_code == 0x0eedfade, "both handlers saw the Delphi exception code");
    // Unwind to the outer frame: the inner handler is called with EXCEPTION_UNWINDING and unlinked.
    g_handler_calls = 0;
    call_import(&c, "KERNEL32.dll", "RtlUnwind", {outer, 0, 0, 0});
    check(g_handler_calls == 1 && (g_seen_flags & 2), "the inner handler was unwound");
    check(rd32(c.fs_base) == outer, "FS:[0] now points at the target frame");
}
```

- [ ] **Step 6: Implement the runtime side** (`seh.cpp`: `seh_raise`, `seh_unwind`, `recomp_seh_frame_enter/leave`, `seh_resume` hooked from `recomp_jump`'s unknown-target path when a frame record matches ESP, `recomp_seh_bad_landing`, `seh_set_unhandled_hook`), point `k_RaiseException`/`k_RtlUnwind` at it, add the CTest entry, run:

`.venv/bin/python tools/test.py --native -R "seh_tests|runtime_tests" > build/seh.log 2>&1; grep -c "\[FAIL\]" build/seh.log` → 0; `.venv/bin/python -m pytest -q tools/recomp/tests/test_translate_seh.py tools/recomp/tests/test_translate_insns.py` → all passed.

- [ ] **Step 7: Format and commit (kit)**

```bash
.venv/bin/python kit/tools/format.py --write
git -C kit add docs/superpowers/specs/2026-09-14-seh-design.md tools/recomp/translate.py tools/recomp/tests/test_translate_seh.py runtime/seh.cpp runtime/seh.h runtime/kernel32.cpp runtime/x86.h runtime/CMakeLists.txt runtime/tests/seh_tests.cpp
git -C kit commit -m "SEH: Delphi frames become host setjmp landings; RaiseException walks the chain, RtlUnwind unlinks"
```

---

## Phase C: the game boots and plays on macOS

Every task in this phase is run-report driven. The loop is always the same: build, run one of the kit's hosts with the switches named, read `build/run.log`, and turn each finding into a fix by the rule given. A task ends when its acceptance frame exists, not when the log looks quiet.

### Task 13: First run under the headless host: through the Delphi runtime's initialization

**Files:**
- Modify: game repo `docs/analysis.md` (run log), `CHANGELOG.md`; kit files as the findings dictate, each in its own kit commit

**Interfaces:**
- Consumes: Tasks 1-12 on `siege-delphi`; `tools/build.py --regenerate --target headless`.
- Produces: `pop_headless` running `Siege.exe` from the entry point into `TApplication.Run`'s message loop without aborting.

- [ ] **Step 1: Build and run**

```bash
cd ~/Documents/Tests/siege-of-avalon-recomp
.venv/bin/python tools/build.py --regenerate --target headless --jobs 8 > build/build-headless.log 2>&1; echo "exit $?"
RECOMP_HEADLESS_FRAMES=600 build/recomp/pop_headless > build/run.log 2>&1; echo "exit $?"
```

(`kit/host/headless_main.cpp` documents its switches at the top of the file; use the frame-count switch it names if it differs.)

- [ ] **Step 2: Read the log by class and fix by rule**

`grep -E "no shim registered|not an allocated trampoline|unknown target|aborting|LoadLibrary.*missing" build/run.log | sort | uniq -c | sort -rn | head -40`

- `GetProcAddress(x, "Y") -> 0 (no shim registered)`: an import the runtime lacks; add the shim in the file that owns `x` (Tasks 7-9 files), with a `runtime_tests` check, kit commit.
- `LoadLibraryA("z"): no shims for that module`: expected for `soaddraw.dll`, `dfx_*.dll`, `d3d11.dll`, `mf.dll`, `mfplat.dll`, `uxtheme.dll`, `dwmapi.dll`, `shcore.dll`, `windowscodecs.dll`, `wtsapi32.dll`; the game must proceed past each. If it aborts after one, the Delphi delay-load helper raised; check that the raise reaches a frame (Task 12) and that the VCL guards the call with an OS-version check the kit answers (`GetVersionExW` should report 6.1 so DPI and theming paths stay off; adjust in `k_GetVersionExW`).
- `RaiseException(code=0eedfade ...)`: a Delphi exception. With Task 12 it is dispatched; if it ends in `recomp_seh_unhandled`, the log prints the exception class name (Delphi's record `ExceptionInformation[1]` is the object; its class name is at `[[obj] - 0x2c]` pointing at a short string, print it in `seh_unhandled`) and the message: the cause is whatever shim answered wrongly just before; fix that shim.
- `call to unknown target`: a translator gap; treat as Task 4 Step 1.
- `divide error` or a `fn_...` crash under `lldb`: run `lldb -- build/recomp/pop_headless` with `RECOMP_HEADLESS_FRAMES=600`, `bt`, read the guest address from the frame name, open the `.asm`, decide translator or shim.

- [ ] **Step 3: Acceptance**

`grep -c "PeekMessageW\|MsgWaitForMultipleObjectsEx" build/run.log` with `RECOMP_LOG_IMPORTS=1` (or the kit's verbose import switch named in `runtime/imports.cpp`) shows the message loop spinning for the 600 frames, and the run exits 0.

- [ ] **Step 4: Record and commit**

Append the run log entry (kit commits, what each finding was, the switch set used) to `docs/analysis.md`; a `CHANGELOG.md` line "The Delphi runtime initialises and the VCL message loop runs under the headless host"; bump `kit`.

```bash
git add kit docs/analysis.md CHANGELOG.md && git commit -m "First run: the runtime initialises and the message loop runs under the headless host"
```

### Task 14: The main menu draws: DirectDraw through the run-time module path, the VCL form, the first frame

**Files:**
- Create: game repo `smoke/main-menu.script`
- Modify: kit `dx/ddraw.cpp` and `runtime/gdi32*.cpp` as the run report dictates; `docs/analysis.md`, `CHANGELOG.md`

**Interfaces:**
- Consumes: the smoke host's script grammar (`wait <ms>`, `click <left|right> <x> <y>`, `key <NAME> <down|up>`, `button <left|right|middle> <down|up>`, `dump <name>`) as `majesty-recomp/smoke/freestyle-beginner.script` uses it; the switches `RECOMP_SCRIPT`, `RECOMP_HOST_DUMP_DIR`, `RECOMP_DDRAW_MODES`, `RECOMP_SMOKE_DRAWABLE`.
- Produces: `build/smoke/main-menu.ppm` (the dump), a frame showing the Siege of Avalon title screen with its menu.

- [ ] **Step 1: Write the script**

`smoke/main-menu.script`:

```text
# From process start to the main menu, at the game's 800x600.
#   RECOMP_SCRIPT=$PWD/smoke/main-menu.script RECOMP_HOST_DUMP_DIR=build/smoke \
#   RECOMP_DDRAW_MODES=800x600x16,800x600x32 RECOMP_SMOKE_DRAWABLE=800x600 build/recomp/pop_smoke
wait 6000
dump main-menu
wait 1000
```

- [ ] **Step 2: Build the smoke host and run**

```bash
.venv/bin/python tools/build.py --target smoke --jobs 8 > build/build-smoke.log 2>&1; echo "exit $?"
mkdir -p build/smoke
RECOMP_SCRIPT=$PWD/smoke/main-menu.script RECOMP_HOST_DUMP_DIR=$PWD/build/smoke RECOMP_DDRAW_MODES=800x600x16,800x600x32 RECOMP_SMOKE_DRAWABLE=800x600 build/recomp/pop_smoke > build/run-smoke.log 2>&1; echo "exit $?"
.venv/bin/python kit/tools/recomp/ppm_to_png.py build/smoke/main-menu.ppm build/smoke/main-menu.png
```

- [ ] **Step 3: Read the log by class and fix by rule**

- `DDRAW.dll!IDirectDraw...::<Method>` reported by the DirectX return observer as failing (`E_NOTIMPL` or the kit's "unimplemented method" line): implement the surface or device method in `dx/ddraw.cpp` in the shape of its neighbours, with a `dx_tests` case; kit commit. The game asks for `IDirectDraw7` first (`Winapi.DirectDraw`'s `DirectDrawCreateEx`?): if the log shows `DirectDrawCreateEx` unresolved, add it as an alias of `DirectDrawCreate` that refuses `IID_IDirectDraw7` with `DDERR_UNSUPPORTED` (the game's own fallback then asks for 4; if it does not, `IDirectDraw7` becomes a kit item recorded in the run log and this task stops until the kit has it).
- A black or partial frame: the game blits its DirectDraw back buffer into the VCL form's client area with `IDirectDrawSurface::Blt` or through GDI (`GetDC` on a surface then `BitBlt` to the window DC). Compare `grep -c "BitBlt\|StretchBlt" build/run-smoke.log` against `grep -c "Surface.*::Blt\|Flip" build/run-smoke.log` to learn which; the frame the smoke host dumps must be the surface the game presents to (the primary, or the window surface of Task 10 when the game draws with GDI). If both are drawn, the compositor rule is: the window surface is the base, a DirectDraw primary composites over its client rectangle.
- Text missing from the menu while buttons show: `ExtTextOutW` reached with a font the game loaded through `AddFontMemResourceEx` (its own `.ttf` in resources); the bitmap font stands in, so text renders in the wrong face but renders. Record it; the face is out of scope.

- [ ] **Step 4: Acceptance**

`build/smoke/main-menu.png` shows the title screen with its menu items; attach its description (not the image) to the run log. `python3 -c "from PIL import Image; im = Image.open('build/smoke/main-menu.png'); print(im.size, len(set(im.getdata())))"` prints `(800, 600)` and more than 1000 distinct colours.

- [ ] **Step 5: Record and commit** (run log, changelog line "The main menu draws under the smoke host", `smoke/main-menu.script`, `kit` pointer).

### Task 15: Into the game: character creation, the first map, sound and music; saves in the profile

**Files:**
- Create: `smoke/new-game.script`
- Modify: `game.toml` (`[bundle]` decisions), `docs/analysis.md`, `CHANGELOG.md`; kit as dictated

**Interfaces:**
- Produces: `build/smoke/first-map.ppm` showing the player standing in the first map (the Outer Bailey), FMOD sound started (`_FSOUND_Init@12` returned 1 and at least one `_FSOUND_PlaySoundAttrib@20` and one `_FSOUND_Stream_OpenMpeg@8` in the log), and a save written under `build/recomp/profile` (the kit's write tier) rather than under `original/gog`.

- [ ] **Step 1: Find the menu coordinates**

From `build/smoke/main-menu.png`, read the pixel positions of "New Game" (the coordinates are guest pixels at 800x600); then iterate the script one screen at a time, dumping after each click, until the map appears. The character creation screen accepts defaults with its Continue button; the intro movie is skipped by the game's own path because `mfplat.dll` is reported missing (verify: `grep "Media Foundation\|mfplat" build/run-smoke.log`).

- [ ] **Step 2: Write the script**

```text
# Main menu -> New Game -> character creation (defaults) -> the first map.
# Coordinates are guest pixels at 800x600 read off build/smoke/*.png dumps.
wait 6000
click left <NEW_GAME_X> <NEW_GAME_Y>
wait 3000
dump character
click left <CONTINUE_X> <CONTINUE_Y>
wait 12000
dump first-map
key F2 down
wait 100
key F2 up
wait 2000
dump saved
```

Replace the four placeholders with the numbers read in Step 1 before committing; a script with `<...>` left in it must not be committed.

- [ ] **Step 3: Run and fix by rule**

Same loop as Task 14. New classes:
- `fmod.dll!_FSOUND_Sample_LoadWav@12` returning 0: the RIFF reader refused a file; print the path and the chunk it stopped at (`LOGW` in `fmod.cpp`), extend the reader (the game's `.wav` files include 8-bit and stereo forms), `dx_tests` case, kit commit.
- A save under `original/gog/savegame`: the write tier missed a path form; the game builds it from `SHGetFolderPathW` (Task 8) or from its executable's directory. Check which with `grep -E "CreateFileW.*\.sav|CreateDirectoryW" build/run-smoke.log`; the kit's file seam must send both to the profile. Kit fix in the seam, `runtime_tests` check.
- The game asks the display for a mode not in `RECOMP_DDRAW_MODES`: `siege.ini`'s `ScreenResolution` decides; the smoke run seeds `siege.ini` with `ScreenResolution=800` through the profile before the run (add the seed to the script's header comment as a shell line, and to `docs/testing.md`).

- [ ] **Step 4: Bundle decisions**

If `grep -c "Media Foundation" build/run-smoke.log` shows the game skipping the movies cleanly, add `"Movies"` to `[bundle].exclude` in `game.toml` with the reason, update `tests/test_game_config.py` (`Movies/SiegeOpening.wmv` becomes excluded) and `required_dirs` (drop `Movies`) and `CONTRIBUTING.md`'s directory list; `python -m pytest -q tests`.

- [ ] **Step 5: Record and commit** (run log with the sound and save evidence, changelog "A new game reaches the first map with sound and music under the smoke host; saves land in the profile").

### Task 16: The macOS app plays

**Files:**
- Modify: `README.md` (Status, Build on macOS), `docs/analysis.md`, `CHANGELOG.md`, `docs/testing.md`

**Interfaces:**
- Consumes: `tools/build.py` (default target `app`) → `build/SiegeOfAvalonRecomp.app`.
- Produces: the app boots to the menu, mouse and keyboard play, Exit closes it, `siege.ini` and saves persist in the app's profile across launches.

- [ ] **Step 1: Build and launch**

```bash
.venv/bin/python tools/build.py --jobs 8 > build/build-app.log 2>&1; echo "exit $?"
open build/SiegeOfAvalonRecomp.app; sleep 20; log show --last 30s --predicate 'process == "SiegeOfAvalonRecomp"' > build/app.log 2>&1
```

- [ ] **Step 2: Play by hand and record**

Click New Game, create a character, walk with the mouse, open the inventory (`I`), talk to the first NPC, save (`F2`), quit, relaunch, load (`F3`). Each thing that does not work is a finding for the same rules as Tasks 13-15, plus: keyboard shortcuts not arriving mean `WM_KEYDOWN`/`WM_CHAR` for the W window need the input gate's key path (`host/input_gate.cpp`) to post to the VCL window; the cursor drawn twice means the game's own cursor sprite and the host pointer are both visible, so hide the host pointer while the game window has focus (the kit's `ShowCursor(FALSE)` path).

- [ ] **Step 3: Acceptance and docs**

The five actions above work; write them into the run log with the date. Update `README.md`'s Status heading to "Status: plays on macOS" with a paragraph in the style of majesty-recomp's, and `docs/testing.md`'s table row for `--gameplay` to name `smoke/new-game.script`.

```bash
git add README.md docs CHANGELOG.md kit && git commit -m "The game plays on macOS: menu, new game, first map, save and load"
```

---

## Phase D: iPad

### Task 17: The iOS build, bundle and first launch on the device

**Files:**
- Modify: `game.toml` (`[touch] keypad`, `[bundle].exclude`), `README.md` (Play on an iPad), `docs/analysis.md`, `CHANGELOG.md`

**Interfaces:**
- Consumes: `tools/build.py --target ios --team <TEAM_ID> --device <DEVICE_ID> --console`; the kit's device workflow from majesty-recomp's `docs/analysis.md` run log "playing on the iPad by hand" (console through `xcrun devicectl device process launch --console`, screenshots through `pymobiledevice3 developer dvt screenshot`).
- Produces: `SiegeOfAvalonRecomp.app` installed and booting to the main menu on the iPad.

- [ ] **Step 1: Bundle size and exclusions**

`du -sh original/gog/ArtLib original/gog/Interface original/gog/Maps` (892 MB, 87 MB, 31 MB). The bundle carries every language's `Interface/<language>` and `ArtLib/Resources/Database/<language>`; keep them (the game's `LanguagePath` setting selects one at run time). With `Movies` excluded after Task 15 the bundle is about 1.0 GB, which the kit seeds into Documents on first launch as it does for Majesty (724 MB); confirm the seeding time on the device is under two minutes and record it.

- [ ] **Step 2: Build, install, launch**

```bash
export RECOMP_IOS_TEAM=<TEAM_ID>
.venv/bin/python tools/build.py --target ios --device <DEVICE_ID> --jobs 8 > build/build-ios.log 2>&1; echo "exit $?"
perl -e 'alarm 900; exec @ARGV' xcrun devicectl device process launch --device <DEVICE_ID> --terminate-existing --console dev.recompkit.siege > build/ios-console.log 2>&1
```

- [ ] **Step 3: Fix by rule**

- The app exits at once with the loader's hash error: the bundle staged a different `Siege.exe` (`[bundle].exclude` must not match it; it does not today).
- iOS-only crashes in the console (`EXC_BAD_ACCESS` in a kit frame) go to the kit with a `bt` from the crash log (`xcrun devicectl device info crashlogs`); everything guest-side is the same code as macOS.
- Display: the game's 800x600 letterboxed by the kit's presenter; if the VCL form comes up windowed and small, force `Windowed=0` in `siege.ini` through the bundle's seeded profile (the kit seeds `Documents/switches.txt`; for a settings file, the game repo adds `profile-seed/siege.ini` and `game.toml` a `[profile] seed = "profile-seed"` key only if the kit has one, else the first-launch page sets it: check `kit/tools/stage_game_files.py` for a seed hook before inventing one).

- [ ] **Step 4: Acceptance and docs**

The main menu shows on the device; a tap on New Game opens character creation. Write the run log entry; update `README.md` "Play on an iPad" from "Not yet" to the working instructions with `<TEAM_ID>`/`<DEVICE_ID>` placeholders.

### Task 18: Playing by touch: the RPG's pointer, hotkeys and inventory on a tablet

**Files:**
- Modify: `game.toml` (`[touch] keypad = "auto"` stays unless the keypad covers the game's bottom bar), `docs/analysis.md`, `CHANGELOG.md`; kit `host/input_touch.cpp` only if a gesture is missing for this game

**Interfaces:**
- Consumes: the kit's gestures (tap = left click, long press then lift = right click, drag = left drag, two-finger drag = arrow keys, two-finger tap = Escape, three-finger tap = F10, on-screen keypad).
- Produces: a run log entry listing which of the game's actions work by touch: walking (tap on the map), attacking (tap on an enemy), the inventory (`I` on the keypad), talking (tap on an NPC), the journal (`J`), save and load through the game's own menu (Escape).

- [ ] **Step 1: Play and list**

Tap through the first map for ten minutes with the console streaming; note every action that fails and why (the log line, or "no response"). The bottom bar's 800x600 layout at the display's scale puts the inventory and spell buttons within a finger's reach; if the keypad's default corners cover the bar's ends, set `[touch] keypad = "hidden"` and note the three-finger tap alternative.

- [ ] **Step 2: Fix what is the kit's**

A tap that the game reads as a click at the wrong point is the mapper (`host/input_touch.cpp`, the fixes recorded for Majesty apply); a hotkey the keypad sends that the game ignores is `WM_KEYDOWN` reaching a W window (Task 16's rule).

- [ ] **Step 3: Record and commit** (run log, changelog "Plays on the iPad by touch", README's iPad paragraph names the gestures that matter for this game).

---

## Phase E: Linux and Windows

### Task 19: `tools/build.py` builds the hosts on Linux and Windows

**Files:**
- Modify: `kit/tools/build.py` (`MACOS_ONLY` at line 35 and the check at line 226)
- Test: `kit/tests/test_build_py.py`

**Interfaces:**
- Today `MACOS_ONLY = {"app", "smoke", "headless", "ios"}` refuses those targets off macOS, yet the kit's CI links `recomp_app`, `pop_headless` and `pop_smoke` on `ubuntu-24.04` and `windows-2025` through the `linux-stub` and `windows-stub` presets. The refusal is stale for the three desktop hosts; `ios` stays macOS-only.

- [ ] **Step 1: Write the failing test**

In `kit/tests/test_build_py.py`, next to the existing `parse_args` tests:

```python
def test_desktop_hosts_build_off_macos(tmp_game):
    for target in ("app", "smoke", "headless"):
        args, _ = build.parse_args(["--game-dir", str(tmp_game), "--target", target], system="Linux")
        assert args.target == target
        args, _ = build.parse_args(["--game-dir", str(tmp_game), "--target", target], system="Windows")
        assert args.target == target

def test_ios_stays_macos_only(tmp_game):
    with pytest.raises(SystemExit):
        build.parse_args(["--game-dir", str(tmp_game), "--target", "ios", "--team", "T"], system="Linux")
```

(`tmp_game` is whatever fixture the file uses for a directory with a `game.toml`; reuse it.)

- [ ] **Step 2: Run to see it fail**: `.venv/bin/python -m pytest -q kit/tests/test_build_py.py -k "desktop_hosts or ios_stays" 2>&1 | tail -3`. Expected: `SystemExit` for the Linux `app` case.

- [ ] **Step 3: Implement**: `MACOS_ONLY = {"ios"}`; the error message becomes `"The ios target builds on macOS"`. Also `default_preset` already maps `Linux`→`linux`, `Windows`→`windows`.

- [ ] **Step 4: Run** the file: all passed. **Step 5: Format, commit (kit)**: `git -C kit commit -am "build.py: the desktop hosts build wherever their preset configures"`.

### Task 20: Linux: the smoke host draws the same main menu

**Files:**
- Modify: `.github/workflows/checks.yml` (game repo: add `windows-2025` to the matrix, as the kit's CI has), `docs/analysis.md`, `CHANGELOG.md`, `README.md` (Build on Linux); kit `platform/os_posix.cpp`, `host/gpu/vulkan/*` as findings dictate

**Interfaces:**
- Consumes: a Linux machine or container with clang, lld, the SDL3 build dependencies and a Vulkan device (the kit's CI installs `mesa-vulkan-drivers` and runs the `gpu` label under lavapipe with `SDL_VIDEODRIVER=dummy`); the game files copied to `original/gog` there (rsync from the Mac; 1.1 GB).
- Produces: `build/smoke/main-menu.ppm` on Linux that `kit/tools/recomp/compare_frames.py` judges equal to the macOS dump within its tolerance.

- [ ] **Step 1: Translate and build on Linux**

```bash
python3 -m venv .venv && .venv/bin/python -m pip install -r kit/requirements-dev.txt
.venv/bin/python tools/build.py --regenerate --target smoke --jobs "$(nproc)" > build/build-linux.log 2>&1; echo "exit $?"
```

Expected findings and rules: a compile error in `chunk_*.c` under Linux clang that macOS clang accepted is an emitter portability bug (fix in `translate.py`, regenerate); a link error for a symbol under `platform/` is a POSIX gap in `os_posix.cpp` (the file is shared with macOS, so this is unlikely and would be an `#ifdef __APPLE__`); `pop_link_sdl` fetching SDL3 needs network on the first configure.

- [ ] **Step 2: Run the smoke script**

Same command as Task 14 Step 2 with `SDL_VIDEODRIVER=dummy` when there is no display. Compare: `.venv/bin/python kit/tools/recomp/compare_frames.py build/smoke/main-menu.ppm <macos-dump>/main-menu.ppm` (read the script's `--help` for its threshold flag; record the score).

- [ ] **Step 3: Fix by rule**

- Case-sensitivity: a file the game opens as `ArtLib\Resources\...` with a different case than on disk is a seam bug: the kit's spec (section 4.4) says every path is normalized against a case-folded index; if a `CreateFileW` fails on Linux and not macOS, the index missed the W path (Task 7's `create_file_named` must go through the same normalizer as the A shim).
- Vulkan present differences (a frame offset or a flipped image): `host/gpu/vulkan` versus Metal; the compositor tests under the `gpu` label are where the fix is tested.

- [ ] **Step 4: CI and docs**

Add to the game repo's `checks.yml` matrix `- os: windows-2025` (the stub configure only; the Windows step needs `ilammy/msvc-dev-cmd@v1` as the kit's workflow shows, copy that step). Write `README.md` "Build on Linux" with the apt line from the kit's CI. Commit: `git add .github README.md docs CHANGELOG.md kit && git commit -m "Linux: the smoke host draws the main menu; CI configures on Windows too"`.

### Task 21: Windows: the smoke host draws the same main menu

**Files:**
- Modify: `docs/analysis.md`, `CHANGELOG.md`, `README.md` (Build on Windows); kit `platform/os_win32.cpp`, `translate.py` as findings dictate

**Interfaces:**
- Consumes: a Windows machine with Visual Studio 2022 Build Tools (the kit's CI enters the developer environment and builds with Ninja and MSVC), Python 3.9+, the game installed by GOG's own installer (its default `C:\GOG Games\Siege of Avalon - Anthology` is the `guest_root` in `game.toml`; link it with `tools\setup.py --install "C:\GOG Games\Siege of Avalon - Anthology" --link-only`).
- Produces: `build\smoke\main-menu.ppm` equal to the macOS dump within tolerance.

- [ ] **Step 1: Translate and build**

```bat
py -3 -m venv .venv && .venv\Scripts\python -m pip install -r kit\requirements-dev.txt
.venv\Scripts\python tools\build.py --regenerate --target smoke --jobs 8 > build\build-windows.log 2>&1
```

Expected findings: MSVC rejects a GNU construct in the generated C (statement expressions, `__attribute__`, computed `goto`): the emitter has to produce ISO C for the generated chunks or the presets have to select clang-cl on Windows; check which the kit's `windows` preset uses (`CMakePresets.json`: if it names `clang-cl`, the second is already the case and only genuine portability bugs remain). Each is a `translate.py` fix with a `test_translate_insns.py` case that compiles under `clang -std=c11 -pedantic` so macOS catches it too.

- [ ] **Step 2: Run, compare, fix by rule** as Task 20; Windows-specific: `platform/os_win32.cpp` path handling for the game directory with spaces, and the host's console output encoding for the log.

- [ ] **Step 3: Docs and commit** ("Windows: the smoke host draws the main menu").

### Task 22: Play sessions on Linux and Windows, and desktop packages

**Files:**
- Create: game repo `tools/package.py` (a four-line wrapper over `kit/tools/recomp/package.py` passing `--game-dir`)
- Modify: `README.md` (Play on Linux and Windows; Packages), `docs/analysis.md`, `CHANGELOG.md`, `NOTICE` (if the packages carry anything new)

**Interfaces:**
- Consumes: `kit/tools/recomp/package.py --game-dir <abs> --preset linux|windows|macos --version v0.1.0` → `<App>-v0.1.0-linux-x64.tar.gz`, `<App>-v0.1.0-windows-x64.zip`, `<App>-v0.1.0-macos-arm64.zip` under `build/`.
- Produces: the app playing the first map by mouse and keyboard on each desktop platform (Task 16's five actions), and one package per platform that runs from a fresh directory next to the player's own game copy.

- [ ] **Step 1: The wrapper**

`tools/package.py`:

```python
#!/usr/bin/env python3
"""Package a finished build of Siege of Avalon: Anthology with the kit in kit/. Every option is the kit's: see kit/tools/recomp/package.py --help."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KIT = ROOT / "kit"
if not (KIT / "tools/recomp/package.py").is_file():
    sys.exit("The kit submodule is missing: run `git submodule update --init`")
sys.exit(subprocess.call([sys.executable, str(KIT / "tools/recomp/package.py"), "--game-dir", str(ROOT)] + sys.argv[1:],
                         cwd=ROOT))
```

- [ ] **Step 2: Play sessions**

On each platform run `tools/build.py` (target `app`) and repeat Task 16 Step 2's five actions; findings follow the same rules; record per platform in the run log.

- [ ] **Step 3: Packages**

Run `tools/package.py --preset <preset> --version v0.1.0` on each platform; unpack each into a fresh directory; run the executable with the game directory linked as its README says; the main menu must appear. `NOTICE` gains nothing unless a package ships a new third-party component (it should not: the packages carry the kit's hosts and documents only).

- [ ] **Step 4: Docs and commit** (`README.md` "Play on Linux and Windows" and "Packages"; changelog "Plays on Linux and Windows; packages for the three desktop platforms").

---

## Phase F: closure

### Task 23: Sentinels become addresses where the bring-up found them

**Files:**
- Modify: `game.toml` (`[hooks]`), `globals.toml`, `tests/test_game_config.py`, `docs/analysis.md`

**Interfaces:**
- The kit's hooks are optional for this game (it runs at the display's rate through `timeSetEvent`, and its cursor is a sprite), so most sentinels stay. Two are worth replacing if the run reports found them: `frame_clock_*` if the game busy-waits on `GetTickCount` (grep the smoke log for a `GetTickCount` call rate above 1 kHz), and `cursor_surface_ptrs` if the game keeps its cursor in a DirectDraw surface whose pointer lives at a fixed global (the log's `IDirectDrawSurface::SetCursor`-shaped calls, if any).

- [ ] **Step 1**: For each replaced sentinel, cite the `.asm` line in a `game.toml` comment; **Step 2**: update `SENTINEL_LOW/HIGH` assertions so replaced entries are exempted by name (`test_unidentified_addresses_stay_in_the_sentinel_padding` skips keys listed in a `VERIFIED = {...}` set with their addresses); **Step 3**: `python -m pytest -q tests`; **Step 4**: commit.

### Task 24: Merge the kit branch and re-pin to main

**Files:**
- Kit: merge `siege-delphi` into `main` in `~/Documents/Tests/recomp-kit`; game repo: `kit` pointer, `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md`

- [ ] **Step 1: Kit checks on the branch**

```bash
cd ~/Documents/Tests/recomp-kit && git checkout siege-delphi && git pull --ff-only ~/Documents/Tests/siege-of-avalon-recomp/kit HEAD
.venv/bin/python tools/test.py && .venv/bin/python tools/format.py && .venv/bin/python tools/check_repo.py && .venv/bin/python tools/check_game_literals.py
.venv/bin/python tools/test.py --game-dir ~/Documents/Tests/majesty-recomp --native > /tmp/majesty-native.log 2>&1; tail -3 /tmp/majesty-native.log
```

Majesty and Populous must still pass their suites: the wide layer, GDI model and SEH are additive, and `runtime_serves_module` now answering from the registry must not change what those games load (both import DirectDraw statically).

- [ ] **Step 2: Merge and push**

```bash
git checkout main && git merge --no-ff siege-delphi -m "Merge siege-delphi: a Delphi VCL game on the kit (SEH, wide API, GDI canvas, FMOD 3, TLS directory)" && git push origin main
```

- [ ] **Step 3: Re-pin and document**

```bash
cd ~/Documents/Tests/siege-of-avalon-recomp && git -C kit fetch ~/Documents/Tests/recomp-kit main && git -C kit checkout FETCH_HEAD
```

`README.md` "Build on macOS": "The submodule is pinned to the kit's `main`". `CONTRIBUTING.md` unchanged. `CHANGELOG.md`: "Kit pinned to `main` <sha> (the `siege-delphi` branch merged)". Commit.

### Task 25: Publish

- [ ] **Step 1**: Ask the user whether the repository goes to GitHub as `veritr1x/siege-of-avalon-recomp` (public like the others). Only with a yes: `gh repo create veritr1x/siege-of-avalon-recomp --public --source . --remote origin --description "Siege of Avalon: Anthology as a native macOS, iPad, Linux and Windows application through recomp-kit" && git push -u origin main`, then `gh repo edit --add-topic recomp --add-topic static-recompilation --add-topic siege-of-avalon` matching majesty-recomp's topics (`gh repo view veritr1x/majesty-recomp --json repositoryTopics`).
- [ ] **Step 2**: Watch the first CI run (`gh run watch`); fix anything the fresh-clone matrix finds (typically a missing `apt` package in the Linux step).

---

## Self-review notes

- Spec coverage: every gap `docs/analysis.md` names has a task: atomics and x87 (1, 2), the four table sites (3), compile (4), TLS directory (5), run-time module loading and the DirectDraw wrapper fallback (6, 14), the `W` layer (6-9), resources and version (7, 8), GDI (10), FMOD, Soundlib, Galaxy (11), SEH (12), Media Foundation movies (15 Step 4), `dfx_*.dll` (13 Step 2: reported missing, the game proceeds), Dfx and Galaxy in bundles (already excluded), the report's literal image base (4), saves in the profile (15), the display resolution setting (15), iPad (17, 18), Linux and Windows (19-22), sentinels (23), pin (24). The delay-loaded DPI, theming and `d3d11` paths have no task because the game must run without them; Task 13 Step 2 says what to do if it does not.
- Unknowns are named as unknowns with a measurement step and a decision rule, never as "TBD": the Soundlib arities (11), the menu coordinates (15), the presentation path (14 Step 3), the MSVC-versus-clang-cl question (21).
- Names used across tasks: `gm_wstr`/`gm_put_wstr` (6, used by 7-10, 12 tests), `imports_has_dll` (6), `loader_tls`/`loader_tls_block_for_thread` (5), `recomp_seh_frame_enter`/`recomp_seh_frame_leave`/`seh_resume`/`recomp_seh_bad_landing` (12), `write_test_riff`/`g_plays`/`tramp`/`call_shim` (11, from `dx_tests.cpp`), `call_import`/`check` (5-9, from `runtime_tests.cpp`), `make_test_window` (10, defined in `gdi_tests.cpp` from Task 9's test body).
