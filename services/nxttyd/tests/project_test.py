#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
source=(root/"src/main.c").read_text()
meta=(root/"project.mk").read_text()
checks=[
    ("main entry", "int main(" in source),
    ("project id", "USERKIT_PROJECT := nxttyd" in meta),
    ("logical name", "USERKIT_SERVICE_NAME := shell" in meta),
    ("service path", "USERKIT_EXECUTABLE := /sys/services/nxttyd" in meta),
]
failed=[name for name,ok in checks if not ok]
if failed:
    print("nxttyd prebuild FAIL: "+", ".join(failed), file=sys.stderr)
    raise SystemExit(1)
print("nxttyd prebuild PASS")
