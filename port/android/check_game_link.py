#!/usr/bin/env python3
"""Attempt the strict game link and preserve every real linker diagnostic.

Exit nonzero on ANY build/link failure. A green component archive build cannot
substitute for this check, nor does a green link prove runtime playability.
"""
import argparse
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import srcpath


def main():
    ap = argparse.ArgumentParser(__doc__)
    ap.add_argument("--build", type=pathlib.Path, required=True)
    ap.add_argument("--cmake", default="cmake")
    args = ap.parse_args()
    command = [args.cmake, "--build", str(args.build), "--target",
               "sm64ds_android_game_link_check", "--parallel", "4"]
    proc = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    log = proc.stdout
    (args.build / "game-link.log").write_text(log)
    undefined = sorted(set(re.findall(r"undefined symbol: (.+)", log)))
    duplicates = sorted(set(re.findall(r"duplicate symbol: (.+)", log)))
    status = "linked_not_runtime_verified" if proc.returncode == 0 else "blocked"
    report = {"status": status, "command": command, "exit_code": proc.returncode,
              "undefined_count": len(undefined), "undefined_symbols": undefined,
              "duplicate_symbols": duplicates, "playable_apk": False}
    # --no-demangle gives stable, exact symbols rather than ambiguous C++
    # display names. Attach original ownership without guessing a substitute.
    owners = []
    for symbol in undefined:
        source = srcpath.path_for(symbol)
        owners.append({"symbol": symbol,
                       "original_source": source.relative_to(ROOT).as_posix() if source else None,
                       "raw_ds_vtable": symbol.startswith(("port_rom_ZTV", "port_vt_"))})
    report["pending_dependencies"] = owners
    (args.build / "game-link.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Game link: {status}; {len(undefined)} undefined, {len(duplicates)} duplicate symbols.")
    print(f"Full diagnostics: {args.build / 'game-link.log'}")
    if proc.returncode:
        for symbol in undefined[:20]:
            print(f"  unresolved: {symbol}")
        if not undefined:
            print(log[-12000:])
    return proc.returncode


if __name__ == "__main__":
    raise SystemExit(main())
