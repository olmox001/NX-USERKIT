#!/usr/bin/env python3
from pathlib import Path
import sys
source = (Path(__file__).resolve().parents[1] / "src/main.c").read_text()
need = ["OS1NX_PORT_NOTIFY", "OS1_port_packet_receive", "example.notify.last"]
missing = [token for token in need if token not in source]
if missing:
    print("nxnotify_registry prebuild FAIL: " + ", ".join(missing), file=sys.stderr)
    raise SystemExit(1)
print("nxnotify_registry prebuild PASS")
