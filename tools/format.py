"""Format first-party files, or check formatting and Python lint with --check.

Requires clang-format, Prettier, and Ruff on PATH. Use --only to select a file
kind and --help for executable overrides. Paths are relative to this repository,
regardless of the working directory. External sources and build files are excluded.
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATTERNS = {
    "cpp": (
        "app/**/*.cpp",
        "src/**/*.cpp",
        "tests/**/*.cpp",
        "app/**/*.hpp",
        "include/**/*.hpp",
        "tests/**/*.hpp",
    ),
    "json": ("assets/**/*.json", "tests/fixtures/**/*.json"),
    "yaml": (".github/**/*.yml", ".github/**/*.yaml"),
    "markdown": ("*.md", "docs/**/*.md"),
    "python": ("tools/**/*.py",),
}


def files_for(kind, root=ROOT):
    return sorted(
        {
            str(path.relative_to(root))
            for pattern in PATTERNS[kind]
            for path in root.glob(pattern)
            if path.is_file()
        }
    )


def commands(args, root=ROOT):
    """Resolve all tools before running any command, avoiding partial formatting."""
    jobs = []
    for kind in args.only or PATTERNS:
        files = files_for(kind, root)
        if not files:
            continue
        if kind == "cpp":
            executable = args.clang_format
            options = ["--dry-run", "--Werror"] if args.check else ["-i"]
        elif kind == "python":
            executable = args.ruff
            options = ["format", "--check"] if args.check else ["format"]
        else:
            executable = args.prettier
            options = ["--check" if args.check else "--write", "--log-level", "warn"]
        tool = shutil.which(executable)
        if tool is None:
            raise FileNotFoundError(
                f"{executable} not found; install it or use an executable override "
                "(see --help)."
            )
        tool = str(Path(tool).absolute())
        jobs.append((kind, [tool, *options, *files]))
        if kind == "python" and args.check:
            jobs.append(("Python lint", [tool, "check", *files]))
    return jobs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="check without edits, including Python lint",
    )
    parser.add_argument(
        "--only", nargs="+", choices=PATTERNS, help="select file kinds (default: all)"
    )
    parser.add_argument(
        "--clang-format", default="clang-format", help="executable or path"
    )
    parser.add_argument("--prettier", default="prettier", help="executable or path")
    parser.add_argument("--ruff", default="ruff", help="executable or path")
    args = parser.parse_args()
    try:
        jobs = commands(args)
    except FileNotFoundError as error:
        print(error, file=sys.stderr)
        return 1
    failed = False
    for label, command in jobs:
        print(f"{'Checking' if args.check else 'Formatting'} {label}", flush=True)
        try:
            result = subprocess.run(command, cwd=ROOT, check=False)
        except OSError as error:
            print(error, file=sys.stderr)
            failed = True
            continue
        failed = failed or result.returncode != 0
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
