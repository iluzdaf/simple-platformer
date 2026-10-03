import argparse
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import format as formatter


class FormattingTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for name in (
            "src/example.cpp",
            "include/example.hpp",
            "external/vendor.cpp",
            "external/README.md",
            "build/generated.cpp",
            "build/generated.json",
            "CMakeUserPresets.json",
            "assets/catalog.json",
            "tests/fixtures/example.json",
            ".github/workflows/ci.yml",
            "README.md",
            "docs/CONTENT.md",
            "tools/example.py",
        ):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()

    def args(self, only=None, check=False):
        return argparse.Namespace(
            only=only,
            check=check,
            clang_format="clang-format",
            prettier="prettier",
            ruff="ruff",
        )

    def test_excludes_vendored_generated_and_personal_files(self):
        selected = {
            path
            for kind in formatter.PATTERNS
            for path in formatter.files_for(kind, self.root)
        }
        self.assertEqual(
            selected,
            {
                "src/example.cpp",
                "include/example.hpp",
                "assets/catalog.json",
                "tests/fixtures/example.json",
                ".github/workflows/ci.yml",
                "README.md",
                "docs/CONTENT.md",
                "tools/example.py",
            },
        )

    @patch("format.shutil.which", side_effect=lambda tool: "/bin/" + tool)
    def test_check_mode_never_requests_edits_and_includes_lint(self, _which):
        jobs = formatter.commands(self.args(check=True), self.root)
        self.assertEqual(jobs[0][1][1:3], ["--dry-run", "--Werror"])
        self.assertEqual(jobs[1][1][1], "--check")
        self.assertEqual(jobs[-2][1][1:3], ["format", "--check"])
        self.assertEqual(jobs[-1][1][1], "check")
        self.assertTrue(
            all("-i" not in cmd and "--write" not in cmd for _, cmd in jobs)
        )

    @patch("format.shutil.which", side_effect=lambda tool: "/bin/" + tool)
    def test_only_selected_kind_requires_its_tool(self, which):
        jobs = formatter.commands(self.args(only=["markdown"]), self.root)
        which.assert_called_once_with("prettier")
        self.assertEqual(jobs[0][1][-2:], ["README.md", "docs/CONTENT.md"])

    @patch("format.shutil.which", side_effect=["/bin/clang-format", None])
    def test_missing_tool_prevents_any_formatting(self, _which):
        with patch("format.subprocess.run") as run:
            with self.assertRaisesRegex(FileNotFoundError, "prettier not found"):
                formatter.commands(self.args(), self.root)
            run.assert_not_called()

    def test_failures_are_reported_while_remaining_checks_run(self):
        jobs = [("cpp", ["clang-format"]), ("markdown", ["prettier"])]
        with (
            patch("sys.argv", ["format.py", "--check"]),
            patch("format.commands", return_value=jobs),
            patch("format.subprocess.run") as run,
        ):
            run.side_effect = [
                argparse.Namespace(returncode=1),
                argparse.Namespace(returncode=0),
            ]
            self.assertEqual(formatter.main(), 1)
            self.assertEqual(run.call_count, 2)
            self.assertEqual(run.call_args.kwargs["cwd"], formatter.ROOT)


if __name__ == "__main__":
    unittest.main()
