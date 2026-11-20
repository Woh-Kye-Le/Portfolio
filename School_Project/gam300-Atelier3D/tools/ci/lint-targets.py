#!/usr/bin/env python3
"""Works out which of our .cpp files clang-tidy has to check for the changes since a commit.

Usage: lint-targets.py <repo root> <build dir> <base commit>   (used by tools/ci/lint.sh)

A file is checked if it changed, or if it includes a changed file, directly or not. The includes come from Ninja's
record of the last build, so the build directory must be a finished Ninja build of the commit being linted.

Prints one anchored regex per file, the form run-clang-tidy takes; nothing at all means no file needs checking.
Exits with FULL_LINT when only a full lint is safe: a change that can affect every file, or missing information.
"""
import functools
import json
import os
import re
import subprocess
import sys

FULL_LINT = 3

# Changing one of these can change the findings of every file, whatever it includes
AFFECTS_EVERY_FILE = re.compile(
    r"(^|/)(CMakeLists\.txt|[^/]*\.cmake|\.clang-tidy|\.clang-format-ignore)$"
    r"|^tools/ci/(build\.sh|lint\.sh|lint-targets\.py)$"
)

real_path = functools.lru_cache(maxsize=None)(os.path.realpath)


def need_full_lint(reason):
    print(f"Full lint: {reason}", file=sys.stderr)
    sys.exit(FULL_LINT)


def run(command, cwd):
    return subprocess.run(command, cwd=cwd, check=True, capture_output=True, text=True).stdout


def changed_files(root, base):
    """Repository-relative paths changed since base; deleted files count, so renames are seen as delete + add."""
    output = run(["git", "diff", "--name-only", "--no-renames", "-z", base, "HEAD"], root)
    return [path for path in output.split("\0") if path]


def our_sources(root, build_dir):
    """Real path -> path as written in the compile database, for the same files a full lint checks."""
    with open(os.path.join(build_dir, "compile_commands.json"), encoding="utf-8") as database:
        entries = json.load(database)
    apps = os.path.join(root, "apps", "")
    skipped = (os.path.join(apps, "desktop", "libraries", ""), os.path.join(apps, "desktop", "build", ""))
    sources = {}
    for entry in entries:
        path = os.path.join(entry["directory"], entry["file"])
        real = real_path(path)
        if real.endswith(".cpp") and real.startswith(apps) and not real.startswith(skipped):
            sources[real] = path
    return sources


def compiled_files(build_dir):
    """One set per object file: every file that went into it, the source file included."""
    groups = []
    for line in run(["ninja", "-t", "deps"], build_dir).splitlines():
        if line.startswith((" ", "\t")):
            if groups:
                groups[-1].add(real_path(os.path.join(build_dir, line.strip())))
        elif ": #deps" in line:
            groups.append(set())
    return groups


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    root = real_path(sys.argv[1])
    build_dir = real_path(sys.argv[2])
    base = sys.argv[3]

    try:
        changed = changed_files(root, base)
        groups = compiled_files(build_dir)
        sources = our_sources(root, build_dir)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        need_full_lint(f"could not work out the changes since {base} ({error})")

    for path in changed:
        if AFFECTS_EVERY_FILE.search(path):
            need_full_lint(f"{path} changed")
    if not groups:
        need_full_lint("Ninja has no include information")

    changed_paths = {real_path(os.path.join(root, path)) for path in changed}
    targets = changed_paths & sources.keys()
    for group in groups:
        if group & changed_paths:
            targets |= group & sources.keys()

    print(f"Changed-file lint: {len(changed)} changed file(s) since {base[:10]}, "
          f"{len(targets)} of {len(sources)} sources to check", file=sys.stderr)
    for target in sorted(targets):
        print(f"^{re.escape(sources[target])}$")


if __name__ == "__main__":
    main()
