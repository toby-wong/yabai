"""Exercise automatic merges in disposable repositories, including conflicts."""

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/sync-upstream.py"


def git(directory, *args):
    return subprocess.check_output(["git", "-C", str(directory), *args], text=True, stderr=subprocess.DEVNULL).strip()


class UpstreamSyncTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="yabai-sync-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.upstream = self.root / "upstream"
        self.fork = self.root / "fork"
        git(self.root, "init", "--initial-branch=master", str(self.upstream))
        self.configure(self.upstream)
        self.commit(self.upstream, "shared.txt", "base\n")
        git(self.root, "clone", str(self.upstream), str(self.fork))
        self.configure(self.fork)
        self.output = self.root / "output"

    def configure(self, repo):
        git(repo, "config", "user.name", "Sync test")
        git(repo, "config", "user.email", "sync-test@example.invalid")
        git(repo, "config", "commit.gpgsign", "false")
        git(repo, "config", "core.hooksPath", "/dev/null")

    def commit(self, repo, name, contents):
        (repo / name).write_text(contents)
        git(repo, "add", name)
        git(repo, "commit", "-m", name)
        return git(repo, "rev-parse", "HEAD")

    def prepare(self, event="schedule"):
        result = subprocess.run(
            [sys.executable, str(SCRIPT), "--event", event, "--upstream", str(self.upstream), "--output", str(self.output)],
            cwd=self.fork, text=True, capture_output=True,
        )
        values = {}
        if self.output.exists():
            values = dict(line.split("=", 1) for line in self.output.read_text().splitlines())
        return result, values

    def test_merge_preserves_fork_commits_without_pushing(self):
        base = self.commit(self.fork, "fork.txt", "native tabs\n")
        upstream = self.commit(self.upstream, "upstream.txt", "new upstream fix\n")
        result, values = self.prepare()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(values["base_sha"], base)
        self.assertEqual(values["upstream_sha"], upstream)
        self.assertEqual(values["build_required"], "true")
        git(self.fork, "merge-base", "--is-ancestor", base, "HEAD")
        git(self.fork, "merge-base", "--is-ancestor", upstream, "HEAD")
        self.assertEqual((self.fork / "fork.txt").read_text(), "native tabs\n")
        self.assertEqual(git(self.upstream, "rev-parse", "HEAD"), upstream)

    def test_conflict_restores_fork_without_pushing(self):
        base = self.commit(self.fork, "shared.txt", "fork's version\n")
        upstream = self.commit(self.upstream, "shared.txt", "upstream's version\n")
        result, _ = self.prepare()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Nothing was pushed", result.stderr)
        self.assertEqual(git(self.fork, "rev-parse", "HEAD"), base)
        self.assertEqual((self.fork / "shared.txt").read_text(), "fork's version\n")
        self.assertEqual(git(self.fork, "status", "--porcelain"), "")
        self.assertEqual(git(self.upstream, "rev-parse", "HEAD"), upstream)

    def test_schedule_skips_unchanged_upstream(self):
        result, values = self.prepare()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(values["build_required"], "false")

    def test_manual_run_rebuilds_unchanged_upstream(self):
        result, values = self.prepare("workflow_dispatch")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(values["build_required"], "true")

    def test_fast_forward_keeps_new_upstream_commit(self):
        upstream = self.commit(self.upstream, "upstream.txt", "fix\n")
        result, values = self.prepare()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(values["source_sha"], upstream)


if __name__ == "__main__":
    unittest.main()
