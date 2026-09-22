#!/usr/bin/env python3
"""Run native Unity suites and report LLVM coverage, without flashing a device."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def llvm_tool(name):
    if sys.platform == "darwin":
        return subprocess.check_output(["xcrun", "--find", name], text=True).strip()
    path = shutil.which(name)
    if not path:
        raise RuntimeError("Install LLVM/Clang; missing tool: " + name)
    return path


def run(args, **kwargs):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pio", default=shutil.which("pio") or str(
        Path.home() / ".platformio/penv/bin/pio"), help="PlatformIO executable")
    args = parser.parse_args()
    cov, profdata = llvm_tool("llvm-cov"), llvm_tool("llvm-profdata")
    suites = sorted(path.parent.name for path in (ROOT / "test").glob("test_*/test_main.cpp"))
    if not suites:
        raise RuntimeError("No native test suites found")
    output = ROOT / ".pio/coverage"
    output.mkdir(parents=True, exist_ok=True)
    # Each run has its own binaries and profiles: previous runs cannot inflate coverage.
    report = Path(tempfile.mkdtemp(prefix="run-", dir=output))
    binaries = []
    for suite in suites:
        raw = report / suite
        raw.mkdir()
        environment = dict(os.environ, LLVM_PROFILE_FILE=str(raw / "%p-%m.profraw"))
        run([args.pio, "test", "-e", "coverage", "-f", suite], env=environment)
        binary = report / (suite + ".bin")
        shutil.copy2(ROOT / ".pio/build/coverage/program", binary)
        binaries.append(binary)
    profiles = sorted(report.glob("*/*.profraw"))
    if not profiles:
        raise RuntimeError("Tests did not produce coverage profiles")
    merged = report / "coverage.profdata"
    run([profdata, "merge", "-sparse", *profiles, "-o", merged])
    common = [binaries[0], "-instr-profile=" + str(merged)]
    for binary in binaries[1:]:
        common.extend(["-object", binary])
    # Keep only project implementation/headers; exclude Unity, tests and system headers.
    common.append("-ignore-filename-regex=" + r"(^|/)(test|\.pio)/|^/Applications/|^/usr/|^/Library/")
    with (report / "summary.txt").open("w") as stream:
        run([cov, "report", *common], stdout=stream)
    with (report / "coverage.json").open("w") as stream:
        run([cov, "export", *common], stdout=stream)
    with (report / "coverage.lcov").open("w") as stream:
        run([cov, "export", *common, "-format=lcov"], stdout=stream)
    run([cov, "show", *common, "-format=html", "-output-dir=" + str(report / "html"),
         "-show-line-counts-or-regions", "-show-branches=count"])

    data = json.loads((report / "coverage.json").read_text())
    measured = set()
    for unit in data["data"]:
        for entry in unit["files"]:
            path = Path(entry["filename"]).resolve()
            if path.is_relative_to(ROOT):
                measured.add(str(path.relative_to(ROOT)))
    inventory = {str(path.relative_to(ROOT)) for folder in ("src", "include")
                 for path in (ROOT / folder).rglob("*")
                 if path.suffix in {".cpp", ".c", ".hpp", ".h"}}
    scope = {
        "suites": suites,
        "measured_files": sorted(measured),
        "files_without_coverage_data": sorted(inventory - measured),
        "note": "Host build only. Missing files and ARDUINO-only branches are NOT measured. "
                "Percentages are not whole-firmware coverage; header declarations may have no executable code.",
    }
    (report / "scope.json").write_text(json.dumps(scope, indent=2) + "\n")
    (output / "latest.txt").write_text(str(report) + "\n")
    print((report / "summary.txt").read_text())
    print("Scope:", scope["note"])
    print("HTML:", report / "html/index.html")
    print("Scope inventory:", report / "scope.json")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, OSError) as error:
        print("Coverage failed:", error, file=sys.stderr)
        sys.exit(1)
