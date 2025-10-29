#!/usr/bin/env python3
import subprocess
from collections import defaultdict
from pathlib import Path

# Path to CONTRIBUTORS.md (in repository root)
contributors_file = Path(__file__).parent.parent / "CONTRIBUTORS.md"

def run_git(command):
    """Run a git command and return the output as a list of lines."""
    try:
        result = subprocess.run(
            ["git"] + command, capture_output=True, text=True, check=True
        )
        return result.stdout.strip().split("\n")
    except subprocess.CalledProcessError as e:
        print(f"❌ Error: Git command failed. Make sure you're in a Git repository.")
        print(f"   Command: git {' '.join(command)}")
        print(f"   Error: {e.stderr}")
        raise SystemExit(1)
    except FileNotFoundError:
        print("❌ Error: Git is not installed or not found in PATH.")
        raise SystemExit(1)

def get_contributors():
    """Extract contributors from git log."""
    log_lines = run_git(["log", "--format=%aN|%aE|%ad", "--date=short"])
    contributors = defaultdict(lambda: {"emails": set(), "first": None, "last": None, "count": 0})

    for line in log_lines:
        if "|" not in line:
            continue
        name, email, date = line.split("|")
        name, email = name.strip(), email.strip()
        info = contributors[name]

        info["emails"].add(email)
        info["count"] += 1
        if not info["first"] or date < info["first"]:
            info["first"] = date
        if not info["last"] or date > info["last"]:
            info["last"] = date

    return contributors

def generate_table(contributors):
    """Generate markdown table."""
    header = [
        "# 🌍 Project Contributors",
        "",
        "This file lists all contributors automatically extracted from Git commit history.",
        "Do not edit manually — run `python scripts/update_contributors.py` to refresh.",
        "",
        "| Name | Email | Contributions | First Commit | Last Commit | Reference |",
        "|------|--------|----------------|---------------|--------------|-----------|",
    ]
    rows = []
    for name, info in sorted(contributors.items(), key=lambda x: x[0].lower()):
        emails = ", ".join(sorted(info["emails"]))
        # Default empty reference link
        reference = ""
        # Use proper pluralization for commits
        commit_text = f"{info['count']} commit" if info['count'] == 1 else f"{info['count']} commits"
        rows.append(f"| {name} | {emails} | {commit_text} | {info['first']} | {info['last']} | {reference} |")
    return "\n".join(header + rows) + "\n"

def main():
    contributors = get_contributors()
    md = generate_table(contributors)
    contributors_file.write_text(md, encoding="utf-8")
    print(f"✅ Updated {contributors_file} with {len(contributors)} contributors.")

if __name__ == "__main__":
    main()
