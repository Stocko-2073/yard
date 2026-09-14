# Base checkout warning

`../hooks.json` registers `warn_base_checkout.py` for SessionStart,
UserPromptSubmit, PostToolUse, and Stop. It discovers the main checkout via Git's
worktree metadata, then runs a read-only porcelain status check there. Any staged,
unstaged, or untracked changes produce a JSON `systemMessage` warning. Ignored
files and commits ahead of a remote are not dirty working-tree changes.

This is deliberately nonblocking. It never stashes, resets, commits, or moves
files. Git failures produce a diagnostic warning instead of silently passing.
Python 3 and Git must be available on PATH.

After merging this configuration into dev, fast-forward the clean base checkout
and use new worktrees containing the configuration. Existing worktrees need the
change incorporated as well. Trust the project and review the hook definitions
with `/hooks` in the Codex CLI; definitions that have not been trusted are skipped.
Do not assume writing the config enables hooks in an already-running session.

Run the isolated regression checks with:

```sh
python3 .codex/hooks/test_warn_base_checkout.py
```

Official configuration and trust reference:
https://learn.chatgpt.com/docs/hooks
