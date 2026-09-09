#!/usr/bin/env python3
"""Assert that this host can certify frame pacing, and record what it is.

The lane fails closed: a host that cannot be shown to have a hardware GL
driver, an active graphical session, a 60 Hz mode and no competing workload
is not a reference host, and any timing it produces is an investigation.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REQUIRED_TOOLS = ("cmake", "ninja", "python3", "glxinfo", "xrandr")
SOFTWARE_RENDERERS = ("llvmpipe", "softpipe", "swrast", "software rasterizer")
REFRESH = re.compile(r"(\d+\.\d+)\*")


def capture(command: list[str], timeout: int = 15) -> tuple[int, str]:
    try:
        result = subprocess.run(
            command, capture_output=True, text=True, timeout=timeout, check=False
        )
        return result.returncode, (result.stdout or "") + (result.stderr or "")
    except (OSError, subprocess.TimeoutExpired) as error:
        return 1, str(error)


def missing_tools() -> list[str]:
    absent = []
    for tool in REQUIRED_TOOLS:
        code, _ = capture(["which", tool], timeout=5)
        if code != 0:
            absent.append(tool)
    return absent


def graphics_facts(environment: dict) -> dict:
    code, glx = capture(["glxinfo", "-B"])
    renderer = ""
    for line in glx.splitlines():
        if "OpenGL renderer string" in line:
            renderer = line.split(":", 1)[1].strip()
            break
    code_xrandr, xrandr = capture(["xrandr", "--current"])
    refresh_rates = [float(value) for value in REFRESH.findall(xrandr)]
    return {
        "display": environment.get("DISPLAY"),
        "wayland_display": environment.get("WAYLAND_DISPLAY"),
        "renderer": renderer,
        "glxinfo_ok": code == 0,
        "xrandr_ok": code_xrandr == 0,
        "active_refresh_hz": refresh_rates,
    }


def load_average() -> list[float] | None:
    if not hasattr(os, "getloadavg"):
        return None
    return list(os.getloadavg())


def evaluate(
    facts: dict,
    competitors: list[dict],
    absent_tools: list[str],
    max_load: float,
) -> list[str]:
    failures = []
    if absent_tools:
        failures.append("missing required tools: " + ", ".join(absent_tools))
    if not facts["display"] and not facts["wayland_display"]:
        failures.append("no graphical session: neither DISPLAY nor WAYLAND_DISPLAY set")
    if not facts["glxinfo_ok"] or not facts["renderer"]:
        failures.append("the OpenGL renderer could not be identified")
    elif any(name in facts["renderer"].lower() for name in SOFTWARE_RENDERERS):
        failures.append(
            f"software rendering cannot certify GPU budgets: {facts['renderer']}"
        )
    if not facts["xrandr_ok"]:
        failures.append("the active display mode could not be read")
    elif not facts["active_refresh_hz"]:
        failures.append("no active refresh rate was reported")
    elif not any(59.0 <= rate <= 61.0 for rate in facts["active_refresh_hz"]):
        failures.append(
            "the lane's budgets are stated at 60 Hz; active modes: "
            + ", ".join(f"{rate:g}" for rate in facts["active_refresh_hz"])
        )
    if competitors:
        names = ", ".join(sorted({item["executable"] for item in competitors}))
        failures.append(f"competing game or build processes are running: {names}")
    load = facts.get("load_average")
    if load and load[0] > max_load:
        failures.append(f"one-minute load average {load[0]:.2f} exceeds {max_load:.2f}")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--max-load",
        type=float,
        default=1.0,
        help="reject the host above this one-minute load average",
    )
    parser.add_argument("--output", type=Path, help="write the host record here")
    parser.add_argument(
        "--warn-only",
        action="store_true",
        help="report the verdict but exit 0; the record still says it failed",
    )
    args = parser.parse_args()

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import importlib.util

    spec = importlib.util.spec_from_file_location(
        "pacing_runner", Path(__file__).resolve().parent / "check-frame-pacing.py"
    )
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)

    facts = graphics_facts(os.environ)
    facts["load_average"] = load_average()
    facts["cpu"] = capture(["lscpu"])[1]
    facts["kernel"] = capture(["uname", "-a"])[1].strip()
    competitors = runner.competing_processes({os.getpid()})
    absent = missing_tools()
    failures = evaluate(facts, competitors, absent, args.max_load)

    record = {
        "reference_host": not failures,
        "failures": failures,
        "competitors": competitors,
        "graphics": {key: facts[key] for key in facts if key != "cpu"},
    }
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(record | {"cpu": facts["cpu"]}, indent=2) + "\n"
        )

    if failures:
        print("NOT a reference host:", flush=True)
        for failure in failures:
            print(f"  - {failure}", flush=True)
        return 0 if args.warn_only else 1
    print(f"Reference host ready: {facts['renderer']}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
