# Clean up merged worktrees

Requirements: Python 3.9+, Git, and an authenticated GitHub CLI (`gh`) with access
to `Stocko-2073/yard`. The script runs on macOS/Linux and uses an advisory file
lock to prevent overlapping cleanup processes.

From the base checkout, preview candidates:

```sh
python3 tools/cleanup_worktrees.py
```

Remove eligible worktrees:

```sh
python3 tools/cleanup_worktrees.py --apply
```

Use `--repo /path/to/yard` when running elsewhere. The script discovers the base
checkout through Git, then considers only registered worktrees inside its
`.worktrees/` directory. It never deletes local or remote branches.

Removal requires a clean checkout and a merged PR into `dev` whose source
repository, branch, and recorded head commit match the worktree. This also works
with squash merges. Open PRs on the same branch prevent cleanup. Locked,
detached, protected-branch, missing, and in-progress Git-operation worktrees are
skipped. GitHub failures preserve the worktree and cause a nonzero exit status.
Status and HEAD are checked again immediately before non-forced Git removal.

Ignored build output can be removed along with an eligible worktree. Untracked
files are preserved. The script skips worktrees containing its own source or
current directory. It cannot reliably detect another editor or agent using a
clean worktree; lock worktrees that must be retained with:

```sh
git worktree lock --reason 'active work' .worktrees/<task>
```

Unlock them with `git worktree unlock .worktrees/<task>` when ready for cleanup.
Do not run cleanup concurrently with edits to an unlocked worktree.

This change supplies a command only; it does not install a daily scheduler.
It can later be invoked at noon America/New_York by a terminal-based scheduler.

Validation:

```sh
python3 -B -m unittest discover -s tools/tests -v
```

Tests use temporary real Git worktrees and mocked GitHub responses; they do not
remove project worktrees.
