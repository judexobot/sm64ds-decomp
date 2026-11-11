#!/usr/bin/env python3
"""Execute actual fallthrough hooks as ARM code, not an Android OS test."""
import argparse
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[3]

parser = argparse.ArgumentParser(__doc__)
parser.add_argument("--clang", required=True)
parser.add_argument("--qemu", required=True)
parser.add_argument("--out", type=pathlib.Path, required=True)
args = parser.parse_args()
args.out.parent.mkdir(parents=True, exist_ok=True)
sources = ["src/_ZN3OAM12EnableSubOAMEv.cpp"]
sources += ["src/_ZN8dActor_c" + name + ".cpp" for name in (
    "11OnAttacked1ERS_", "11OnAttacked2ERS_", "19OnHitFromUnderneathERS_",
    "24OnHitByCannonBlastedCharERS_", "8OnPushedERS_")]
sources += ["port/android/tests/native_fallthrough_test.cpp", "port/android/tests/arm_test_start.S"]
command = [args.clang, "-O2", "-static", "-nostdlib", "-fno-exceptions", "-fno-rtti",
           "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-fno-strict-return",
           "-Wl,-e,_start", "-Wl,--gc-sections", "-ffunction-sections", "-fdata-sections",
           "-std=c++17", "-DSM64DS_PLATFORM_ANDROID=1", "-Iport", "-Iinclude", "-Iport/ntr/include",
           *sources, "-o", str(args.out.resolve())]
subprocess.run(command, cwd=ROOT, check=True)
subprocess.run([args.qemu, str(args.out.resolve())], check=True)
print("Actual OAM effect and five actor hooks returned correctly on ARM; Android OS/device not tested")
