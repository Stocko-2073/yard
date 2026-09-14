#!/usr/bin/env python3
"""Warn, without blocking or modifying files, when the main checkout is dirty."""
import json
import os
from pathlib import Path
import subprocess
import sys


def git(cwd, *args):
    # Hook sessions may inherit Git overrides; inspect the requested checkout.
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    env["GIT_OPTIONAL_LOCKS"] = "0"
    return subprocess.check_output(
        ["git", "-C", str(cwd), *args], env=env, stderr=subprocess.PIPE,
        timeout=5,
    )


def main():
    try:
        payload = json.load(sys.stdin)
        cwd = payload.get("cwd") or os.getcwd()
        # The first porcelain record is the main worktree, even when invoked
        # from a linked worktree or one of its subdirectories. NUL delimiters
        # handle spaces, newlines, and non-ASCII names without shell parsing.
        listing = git(cwd, "worktree", "list", "--porcelain", "-z")
        first = listing.split(b"\0", 1)[0]
        if not first.startswith(b"worktree "):
            raise ValueError("cannot identify the base checkout")
        base = Path(os.fsdecode(first[len(b"worktree "):]))
        status = git(base, "status", "--porcelain=v1", "-z", "--untracked-files=normal")
        if status:
            print(json.dumps({"systemMessage": (
                f"Warning: the base checkout at {base} has staged, unstaged, "
                "or untracked changes. Work should be done in feature worktrees "
                "under .worktrees/, with commits and PRs targeting dev. "
                "Preserve existing base-checkout changes; do not reset or move "
                "someone else's work automatically."
            )}))
    except (OSError, subprocess.SubprocessError, ValueError, AttributeError) as error:
        print(json.dumps({"systemMessage": (
            f"Base-checkout warning hook could not check Git status: {error}"
        )}))
    # A warning only: no blocking exit status or decision.


if __name__ == "__main__":
    main()
