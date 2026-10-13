import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from run_versions import _clone_repository

class CloneRegression(unittest.TestCase):
    @unittest.skipUnless(os.geteuid() == 0, "The benchmark image runs as root against a runner-owned bind mount")
    def test_clones_runner_owned_checkout_after_trusting_git_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repository = root / "repository"
            repository.mkdir()
            (repository / "marker").write_text("fixture")
            configuration = root / "gitconfig"
            environment = dict(os.environ, GIT_CONFIG_GLOBAL=str(configuration))
            subprocess.run(["git", "init", str(repository)], check=True, capture_output=True, env=environment)
            subprocess.run(["git", "-C", str(repository), "add", "marker"], check=True, capture_output=True, env=environment)
            subprocess.run(["git", "-C", str(repository), "-c", "user.name=Regression", "-c", "user.email=regression@example.invalid", "commit", "-m", "fixture"], check=True, capture_output=True, env=environment)
            for file in repository.rglob("*"):
                os.chown(file, 65534, 65534)
            os.chown(repository, 65534, 65534)
            # Registering only the worktree reproduces the failed pilot clone.
            subprocess.run(["git", "config", "--global", "--add", "safe.directory", str(repository)], check=True, env=environment)
            before = subprocess.run(["git", "clone", "--shared", "--no-checkout", str(repository), str(root / "before")], capture_output=True, env=environment)
            self.assertNotEqual(before.returncode, 0)
            self.assertIn(b"dubious ownership", before.stderr)
            with patch.dict(os.environ, environment):
                _clone_repository(repository, root / "after")
            body = subprocess.check_output(["git", "-C", str(root / "after"), "show", "HEAD:marker"], env=environment)
            self.assertEqual(body, b"fixture")

if __name__ == "__main__":
    unittest.main()
