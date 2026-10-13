"""Exercise bridge portability against the real upstream rename snapshots."""
import subprocess
import tempfile
import unittest
from pathlib import Path

from prepare_version import prepare


class BridgePortabilityTest(unittest.TestCase):
    def test_namespace_rename_before_header_directory_rename_preserves_header_paths(self):
        repository = Path(__file__).resolve().parents[2]
        versions = (
            "550f64be2232e09934ffc660a9acdbacff8b164a",
            "65a34053706139d5f6c52e71883d58ceda4c077b",
            "2e13de25f46d37240a7fce1e460ba92a0d9b5edb",
        )
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "source"
            subprocess.run([
                "git", "-c", f"safe.directory={repository}", "-c", f"safe.directory={repository / '.git'}",
                "clone", "--no-local", "--no-checkout", str(repository), str(source),
            ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            for version in versions:
                with self.subTest(version=version):
                    subprocess.run(["git", "checkout", "--force", "--detach", version], cwd=source,
                                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                    prepare(repository, source)
                    renderer = (source / "src/mbgl/renderer/renderer.cpp").read_text()
                    self.assertIn("namespace mln", renderer)
                    self.assertIn("#include <mbgl/util/exception.hpp>", renderer)
                    self.assertNotIn("#include <mln/", renderer)


if __name__ == "__main__":
    unittest.main()
