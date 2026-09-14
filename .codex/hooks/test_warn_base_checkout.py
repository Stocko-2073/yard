"""Exercise the warning contract against real, isolated Git worktrees."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HOOK = Path(__file__).with_name('warn_base_checkout.py').resolve()


class HookTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='yard hook ')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name) / 'base checkout'
        self.base.mkdir()
        self.git('init', '-q')
        self.git('config', 'user.name', 'Hook Test')
        self.git('config', 'user.email', 'hook@example.invalid')
        (self.base / '.gitignore').write_text('/.worktrees/\n/build/\n')
        (self.base / 'tracked').write_text('initial\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'Initial')
        self.linked = self.base / '.worktrees' / 'task'
        self.git('worktree', 'add', '-qb', 'task', str(self.linked))
        (self.linked / 'subdir').mkdir()

    def git(self, *args):
        subprocess.run(['git', '-C', str(self.base), *args], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def check_hook(self, cwd=None):
        result = subprocess.run([sys.executable, str(HOOK)],
            input=json.dumps({'cwd': str(cwd or self.linked / 'subdir')}),
            text=True, capture_output=True, check=True)
        self.assertEqual(result.stderr, '')
        return json.loads(result.stdout) if result.stdout else None

    def test_clean_and_ignored_and_linked_changes(self):
        (self.base / 'build').mkdir()
        (self.base / 'build' / 'output').write_text('ignored')
        (self.linked / 'work-in-progress').write_text('legitimate task work')
        self.assertIsNone(self.check_hook())
        self.assertIsNone(self.check_hook(self.base))

    def test_untracked_base_file_warns_from_linked_worktree(self):
        (self.base / 'new file').write_text('keep me')
        message = self.check_hook()['systemMessage']
        self.assertIn(str(self.base), message)
        self.assertIn('Work should be done in feature worktrees', message)
        self.assertEqual((self.base / 'new file').read_text(), 'keep me')

    def test_unstaged_and_staged_changes(self):
        (self.base / 'tracked').write_text('modified\n')
        self.assertIsNotNone(self.check_hook())
        self.git('add', 'tracked')
        self.assertIsNotNone(self.check_hook())
        self.git('commit', '-qm', 'Modified')
        self.assertIsNone(self.check_hook())

    def test_git_failure_warns_without_blocking(self):
        self.assertIn('could not check', self.check_hook(self.temp.name)['systemMessage'])


if __name__ == '__main__':
    unittest.main()
