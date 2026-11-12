#!/usr/bin/env python3
from pathlib import Path
import sys
source = (Path(__file__).resolve().parents[1] / "src/main.c").read_text()
need = ["NXEXAMPLE_ECHO_PORT", "OS1_port_packet_receive", "reply_to_request"]
missing = [token for token in need if token not in source]
if missing:
    print("nxexample_echo prebuild FAIL: " + ", ".join(missing), file=sys.stderr)
    raise SystemExit(1)
print("nxexample_echo prebuild PASS")
