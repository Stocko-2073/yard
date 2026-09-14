#!/usr/bin/env python3
"""Remove clean Yard worktrees with a matching merged PR; dry-run by default."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
from urllib.parse import urlencode


def run(*args):
    env = {key: value for key, value in os.environ.items()
           if not key.startswith('GIT_')}
    env['GIT_OPTIONAL_LOCKS'] = '0'
    return subprocess.check_output(args, env=env, stderr=subprocess.PIPE,
                                   timeout=60)


def git(path, *args):
    return run('git', '-C', str(path), *args)


def worktrees(repo):
    records = []
    for block in git(repo, 'worktree', 'list', '--porcelain', '-z').split(b'\0\0'):
        record = {}
        for field in block.split(b'\0'):
            if field:
                key, _, value = field.partition(b' ')
                record[os.fsdecode(key)] = os.fsdecode(value)
        if record:
            records.append(record)
    return records


def pull_requests(repository, branch):
    owner = repository.split('/')[0]
    query = urlencode({'state': 'all', 'head': f'{owner}:{branch}', 'per_page': 100})
    pages = json.loads(run('gh', 'api', '--hostname', 'github.com', '--paginate',
                          '--slurp', f'repos/{repository}/pulls?{query}'))
    return [pr for page in pages for pr in page]


def merged_pr(prs, repository, branch, head):
    matching = [pr for pr in prs
                if pr['head']['ref'] == branch
                and (pr['head'].get('repo') or {}).get('full_name', '').lower()
                == repository.lower()]
    if any(pr['state'] == 'open' for pr in matching):
        return None
    return next((pr for pr in matching
                 if pr.get('merged_at') and pr['head']['sha'] == head
                 and pr['base']['ref'] == 'dev'), None)


def inside(path, parent):
    return path != parent and parent in path.parents


def inspect(record, base, protected):
    path = Path(record['worktree'])
    resolved = path.resolve()
    root = base / '.worktrees'
    if not inside(path.absolute(), root) or not inside(resolved, root):
        return 'outside base .worktrees directory'
    if path != resolved:
        return 'path contains a symlink'
    if any(p == resolved or inside(p, resolved) for p in protected):
        return 'contains the running script or current working directory'
    if 'locked' in record or 'prunable' in record:
        return 'locked or unavailable worktree'
    if 'branch' not in record:
        return 'detached HEAD'
    if record['branch'] in ('refs/heads/dev', 'refs/heads/main'):
        return 'protected branch'
    if not path.is_dir():
        return 'missing worktree directory'
    if git(path, 'status', '--porcelain=v1', '-z', '--untracked-files=all'):
        return 'staged, unstaged, or untracked changes'
    # Do not remove a clean checkout paused in the middle of a Git operation.
    for marker in ('MERGE_HEAD', 'CHERRY_PICK_HEAD', 'REVERT_HEAD',
                   'rebase-merge', 'rebase-apply', 'sequencer', 'BISECT_START', 'index.lock'):
        marker_path = os.fsdecode(git(path, 'rev-parse', '--git-path', marker)).strip()
        if (path / marker_path).exists():
            return 'Git operation in progress'
    return None


def cleanup(repo, repository, apply=False, protected=None):
    records = worktrees(repo)
    if not records or 'bare' in records[0]:
        raise ValueError('a non-bare main checkout is required')
    base = Path(records[0]['worktree']).resolve()
    root = base / '.worktrees'
    if root.is_symlink():
        raise ValueError('.worktrees must not be a symlink')
    protected = protected if protected is not None else [Path.cwd().resolve(), Path(__file__).resolve()]
    errors = 0
    for record in records[1:]:
        path = Path(record['worktree'])
        # Only registered worktrees under this repository's .worktrees are in scope.
        if not inside(path.absolute(), root):
            continue
        try:
            reason = inspect(record, base, protected)
            if reason:
                print(f'SKIP {path}: {reason}')
                continue
            branch = record['branch'].removeprefix('refs/heads/')
            pr = merged_pr(pull_requests(repository, branch), repository, branch, record['HEAD'])
            if not pr:
                print(f'SKIP {path}: no matching merged PR into dev, or branch has an open PR')
                continue
            if not apply:
                print(f'WOULD REMOVE {path}: merged PR #{pr["number"]}')
                continue
            # Re-read the registry, branch, HEAD, and status immediately before removal.
            current = next((r for r in worktrees(base) if r['worktree'] == str(path)), None)
            if (not current or current.get('HEAD') != record['HEAD']
                    or current.get('branch') != record['branch']
                    or inspect(current, base, protected)):
                print(f'SKIP {path}: worktree changed during verification')
                continue
            git(base, 'worktree', 'remove', str(path))  # Never --force.
            print(f'REMOVED {path}: merged PR #{pr["number"]}')
        except (OSError, subprocess.SubprocessError, ValueError, KeyError, TypeError) as error:
            errors += 1
            print(f'ERROR {path}: {error}; preserved', file=sys.stderr)
    return 1 if errors else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path.cwd(), help='any checkout of Yard')
    parser.add_argument('--github-repo', default='Stocko-2073/yard')
    parser.add_argument('--apply', action='store_true', help='remove eligible worktrees')
    args = parser.parse_args()
    try:
        common = Path(os.fsdecode(git(args.repo, 'rev-parse', '--path-format=absolute',
                                     '--git-common-dir')).strip())
        # Serialize cleanup processes across all linked worktrees.
        with (common / 'yard-cleanup.lock').open('a') as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                print('SKIP: another cleanup process is running')
                return 0
            return cleanup(args.repo, args.github_repo, args.apply)
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
