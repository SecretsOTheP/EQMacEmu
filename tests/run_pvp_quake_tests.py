#!/usr/bin/env python3
"""Run focused quake checks with Python 3 and g++ (no server or database needed).

The fixtures mock database responses, including failures at each transaction
step. Production method bodies are extracted from the checkout being tested,
so the tests cannot silently exercise an outdated copy of the implementation.
"""

import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
METHODS = {
    "cleanup": ("common/database.cpp", "Database", ["AdjustPVPSpawnTimes"]),
    "hardening": ("common/database.cpp", "Database", [
        "SaveNextQuakeTime", "GetAutomaticQuakeTime", "GetPVPZoneAccess",
        "AdjustPVPSpawnTimes",
    ]),
    "access": ("zone/zoning.cpp", "Client", ["CanEnterPvpInstance"]),
}


def extract_method(source, owner, name):
    start = source.index("bool " + owner + "::" + name + "(")
    # These out-of-class definitions close at column zero; nested blocks do not.
    end = source.index("\n}", start) + 2
    return source[start:end]


def main():
    with tempfile.TemporaryDirectory(prefix="pvp-quake-tests-") as directory:
        for name, (path, owner, methods) in METHODS.items():
            source = (ROOT / path).read_text()
            fixture = (ROOT / "tests/pvp_quake" / (name + ".cpp.in")).read_text()
            assert fixture.count("// PRODUCTION_METHODS") == 1
            fixture = fixture.replace("// PRODUCTION_METHODS", "\n\n".join(
                extract_method(source, owner, method) for method in methods
            ))
            cpp = Path(directory) / (name + ".cpp")
            binary = Path(directory) / name
            cpp.write_text(fixture)
            subprocess.run([
                os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra",
                "-I" + str(ROOT), str(cpp), "-o", str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
