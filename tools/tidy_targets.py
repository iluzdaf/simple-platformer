"""List the files clang-tidy needs to inspect for a change.

A changed source affects only itself, but a changed header can affect every file
that includes it, directly or through other headers. This walks the project include
graph backwards and prints that complete set.

With no --since, every first-party C++ source and header is printed. Changes to the
analysis rules or build configuration also select the whole tree.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOTS = ("app", "src", "include", "tests")
RULES = (
    ".clang-tidy",
    ".github/workflows/ci.yml",
    "CMakeLists.txt",
    "tools/tidy_targets.py",
)
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)


def sources():
    return sorted(
        path
        for root in ROOTS
        for path in Path(root).rglob("*")
        if path.suffix in (".cpp", ".hpp")
    )


def included_by(paths):
    """Map each file to the first-party files which include it."""
    resolvable = {}
    for path in paths:
        resolvable.setdefault(path.name, []).append(path)
        for root in ROOTS:
            try:
                resolvable.setdefault(str(path.relative_to(root)), []).append(path)
            except ValueError:
                pass

    users = {}
    for path in paths:
        source = path.read_text(encoding="utf-8", errors="ignore")
        for included in INCLUDE.findall(source):
            for target in resolvable.get(included, []):
                users.setdefault(target, set()).add(path)

    return users


def reaching(changed, users):
    """Return the changed files and every file which transitively includes them."""
    found = set(changed)
    pending = list(changed)
    while pending:
        for user in users.get(pending.pop(), ()):
            if user not in found:
                found.add(user)
                pending.append(user)

    return found


def merge_base(reference):
    result = subprocess.run(
        ["git", "merge-base", reference, "HEAD"],
        capture_output=True,
        text=True,
        check=False,
    )
    return result.stdout.strip() if result.returncode == 0 else reference


def changed_since(reference):
    result = subprocess.run(
        ["git", "diff", "--name-only", "--no-renames", reference, "--"]
        + list(ROOTS)
        + list(RULES),
        capture_output=True,
        text=True,
        check=True,
    )
    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard", "--"]
        + list(ROOTS)
        + list(RULES),
        capture_output=True,
        text=True,
        check=True,
    )
    return [
        Path(name)
        for name in result.stdout.splitlines() + untracked.stdout.splitlines()
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--since", help="git ref to compare against; omit to list everything"
    )
    arguments = parser.parse_args()

    paths = sources()
    if not arguments.since:
        print("\n".join(str(path) for path in paths))
        return 0

    changed = changed_since(merge_base(arguments.since))
    if any(str(path) in RULES for path in changed):
        print("\n".join(str(path) for path in paths))
        return 0

    available = set(paths)
    changed_cpp = [path for path in changed if path.suffix in (".cpp", ".hpp")]
    # The current include graph cannot prove the reach of a removed or renamed path.
    if any(path not in available for path in changed_cpp):
        print("\n".join(str(path) for path in paths))
        return 0

    changed_sources = [path for path in changed_cpp if path in available]
    if not changed_sources:
        return 0

    affected = reaching(changed_sources, included_by(paths))
    print("\n".join(sorted(str(path) for path in affected)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
