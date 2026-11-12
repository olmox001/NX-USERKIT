#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
source=(root/"src/main.c").read_text()
meta=(root/"project.mk").read_text()
checks=[
    ("main entry", "int main(" in source),
    ("project id", "USERKIT_PROJECT := nxntfy_srv" in meta),
    ("logical name", "USERKIT_SERVICE_NAME := notify" in meta),
    ("service path", "USERKIT_EXECUTABLE := /sys/services/nxntfy_srv" in meta),
]
failed=[name for name,ok in checks if not ok]
if failed:
    print("nxntfy_srv prebuild FAIL: "+", ".join(failed), file=sys.stderr)
    raise SystemExit(1)
print("nxntfy_srv prebuild PASS")
