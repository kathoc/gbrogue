#!/usr/bin/env python3
"""Run every tests/verify_*.py in order; stop-on-fail is off so one broken
milestone doesn't hide the state of the others."""
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main() -> int:
    scripts = sorted(HERE.glob("verify_*.py"))
    if not scripts:
        print("no verify scripts found")
        return 1
    failed = []
    for script in scripts:
        print(f"=== {script.name} ===")
        r = subprocess.run([sys.executable, str(script)], cwd=HERE)
        if r.returncode != 0:
            failed.append(script.name)
    print()
    if failed:
        print(f"FAILED: {', '.join(failed)}")
        return 1
    print(f"all {len(scripts)} verify scripts passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
