#!/usr/bin/env python3
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
source = (root / "src/main.c").read_text()
need = ["nxinit_run", "NXINIT_NAME", "NXINIT_BOOT_MESSAGE"]
missing = [token for token in need if token not in source]
if missing:
    print("nxinit prebuild FAIL: " + ", ".join(missing), file=sys.stderr)
    raise SystemExit(1)
print("nxinit prebuild PASS")
