#!/usr/bin/env python3
"""Report upstream changes and likely integration conflicts without modifying the checkout."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--remote", default="upstream")
    parser.add_argument("--branch", default="master")
    args = parser.parse_args()

    try:
        git("config", "--get", f"remote.{args.remote}.url")
    except subprocess.CalledProcessError:
        parser.error(f"git remote does not exist: {args.remote}")

    subprocess.run(["git", "fetch", "--quiet", args.remote, args.branch], cwd=ROOT, check=True)
    upstream = f"{args.remote}/{args.branch}"
    merge_base = git("merge-base", "HEAD", upstream)
    ahead = git("rev-list", "--count", f"{upstream}..HEAD")
    behind = git("rev-list", "--count", f"HEAD..{upstream}")
    changed = git("diff", "--name-only", f"{merge_base}...{upstream}")
    local_changed = git("diff", "--name-only", f"{merge_base}...HEAD")
    working_tree_changed = git("diff", "--name-only")
    local_changed = "\n".join(filter(None, [local_changed, working_tree_changed]))
    overlap = sorted(set(changed.splitlines()) & set(local_changed.splitlines()))

    print(f"remote: {upstream}")
    print(f"merge-base: {merge_base}")
    print(f"local-only commits: {ahead}")
    print(f"upstream-only commits: {behind}")
    print(f"upstream changed files: {len(changed.splitlines()) if changed else 0}")
    print(f"overlapping files needing review: {len(overlap)}")
    for path in overlap:
        print(f"  {path}")
    if overlap:
        print("STATUS: manual conflict review required")
    else:
        print("STATUS: no file-level overlap detected")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        print(f"git command failed: {error}", file=sys.stderr)
        raise SystemExit(error.returncode)
