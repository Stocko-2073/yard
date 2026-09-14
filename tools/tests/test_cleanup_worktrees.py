import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('cleanup', Path(__file__).parents[1] / 'cleanup_worktrees.py')
cleanup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cleanup)


class CleanupTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='yard cleanup ')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name).resolve() / 'base'
        self.base.mkdir()
        self.git(self.base, 'init', '-q')
        self.git(self.base, 'config', 'user.name', 'Test')
        self.git(self.base, 'config', 'user.email', 'test@example.invalid')
        (self.base / '.gitignore').write_text('/.worktrees/\n/build/\n')
        self.git(self.base, 'add', '.')
        self.git(self.base, 'commit', '-qm', 'Initial')
        self.tree = self.base / '.worktrees' / 'task with spaces'
        self.git(self.base, 'worktree', 'add', '-qb', 'task', str(self.tree))
        self.head = self.git(self.tree, 'rev-parse', 'HEAD').strip().decode()
        self.pr = {'number': 4, 'state': 'closed', 'merged_at': '2026-09-14',
                   'head': {'ref': 'task', 'sha': self.head, 'repo': {'full_name': 'Stocko-2073/yard'}},
                   'base': {'ref': 'dev'}}

    def git(self, cwd, *args):
        return subprocess.check_output(['git', '-C', str(cwd), *args], stderr=subprocess.PIPE)

    def execute(self, apply=True, prs=None, protected=None):
        with patch.object(cleanup, 'pull_requests', return_value=prs if prs is not None else [self.pr]), contextlib.redirect_stdout(io.StringIO()):
            return cleanup.cleanup(self.base, 'Stocko-2073/yard', apply, protected or [])

    def test_dry_run_then_removal_preserves_branch(self):
        self.assertEqual(self.execute(False), 0)
        self.assertTrue(self.tree.exists())
        self.assertEqual(self.execute(), 0)
        self.assertFalse(self.tree.exists())
        self.git(self.base, 'show-ref', '--verify', 'refs/heads/task')

    def test_dirty_and_staged_are_preserved(self):
        (self.tree / 'new').write_text('work')
        self.execute()
        self.assertTrue(self.tree.exists())
        self.git(self.tree, 'add', 'new')
        self.execute()
        self.assertTrue(self.tree.exists())

    def test_new_commits_are_preserved(self):
        self.git(self.tree, 'commit', '--allow-empty', '-qm', 'After merge')
        self.execute()
        self.assertTrue(self.tree.exists())

    def test_closed_unmerged_and_open_prs_are_preserved(self):
        for prs in ([], [dict(self.pr, merged_at=None)], [self.pr, dict(self.pr, state='open', merged_at=None)]):
            self.execute(prs=prs)
            self.assertTrue(self.tree.exists())

    def test_locked_and_active_are_preserved(self):
        self.execute(protected=[self.tree / 'subdir'])
        self.assertTrue(self.tree.exists())
        self.git(self.base, 'worktree', 'lock', str(self.tree))
        self.execute()
        self.assertTrue(self.tree.exists())

    def test_detached_is_preserved(self):
        self.git(self.tree, 'checkout', '--detach')
        self.execute()
        self.assertTrue(self.tree.exists())

    def test_api_failure_is_preserved(self):
        with patch.object(cleanup, 'pull_requests', side_effect=ValueError('API failure')), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(cleanup.cleanup(self.base, 'Stocko-2073/yard', True, []), 1)
        self.assertTrue(self.tree.exists())

    def test_branch_changes_during_query_are_preserved(self):
        def query(*args):
            self.git(self.tree, 'commit', '--allow-empty', '-qm', 'Concurrent work')
            return [self.pr]
        with patch.object(cleanup, 'pull_requests', side_effect=query), contextlib.redirect_stdout(io.StringIO()):
            cleanup.cleanup(self.base, 'Stocko-2073/yard', True, [])
        self.assertTrue(self.tree.exists())


if __name__ == '__main__':
    unittest.main()
