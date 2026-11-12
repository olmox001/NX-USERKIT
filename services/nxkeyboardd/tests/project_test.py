#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
source=(root/"src/main.c").read_text()
meta=(root/"project.mk").read_text()
checks=[
    ("main entry", "int main(" in source),
    ("project id", "USERKIT_PROJECT := nxkeyboardd" in meta),
    ("logical name", "USERKIT_SERVICE_NAME := keyboard" in meta),
    ("service path", "USERKIT_EXECUTABLE := /sys/services/nxkeyboardd" in meta),
    ("provider API", "os1_keyboard_translate" in source and "#include <os1_keyboard_layout_provider.h>" in source),
]
failed=[name for name,ok in checks if not ok]
if failed:
    print("nxkeyboardd prebuild FAIL: "+", ".join(failed), file=sys.stderr)
    raise SystemExit(1)
print("nxkeyboardd prebuild PASS")
