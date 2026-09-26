import unittest
from pathlib import Path

from tidy_targets import selected_paths, sources


class TidyTargetSelectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.paths = sources()
        cls.changed_source = Path("src/actor/actor_id.cpp")
        cls.manifest = Path("cmake/sources/Core.cmake")

    def test_source_and_manifest_select_only_changed_code(self):
        self.assertEqual(
            selected_paths(self.paths, [self.changed_source, self.manifest]),
            [self.changed_source],
        )

    def test_manifest_only_change_selects_everything(self):
        self.assertEqual(selected_paths(self.paths, [self.manifest]), self.paths)

    def test_global_build_change_selects_everything(self):
        self.assertEqual(
            selected_paths(self.paths, [Path("CMakeLists.txt")]), self.paths
        )

    def test_removed_source_selects_everything(self):
        self.assertEqual(
            selected_paths(self.paths, [Path("src/removed.cpp")]), self.paths
        )

    def test_unrelated_change_selects_nothing(self):
        self.assertEqual(selected_paths(self.paths, [Path("README.md")]), [])


if __name__ == "__main__":
    unittest.main()
