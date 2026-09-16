"""native/dxr_blend.h computes the blends the game's DXEffects asks for.

The header replaces routines that compile their loops at run time, so nothing
else checks its arithmetic. The harness drives it against hand-made 16-bit
surfaces with the runtime reduced to stubs.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HARNESS = ROOT / "tests" / "native" / "dxr_blend_harness.cpp"


@unittest.skipUnless(shutil.which("c++"), "needs a C++ compiler")
class DxrBlendTests(unittest.TestCase):
    def test_blends(self):
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / "dxr_blend_harness"
            build = subprocess.run(
                ["c++", "-std=c++17", "-Wall", "-Wno-unused-function",
                 "-I", str(ROOT / "native"), "-I", str(ROOT / "kit" / "runtime"),
                 str(HARNESS), "-o", str(exe)],
                capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("0 failures", run.stdout)


if __name__ == "__main__":
    unittest.main()
