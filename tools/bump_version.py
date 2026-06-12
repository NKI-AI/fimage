#!/usr/bin/env python3
"""Interactively bump the fim (FastImage) version across the repository.

This tool finds and updates fimage's version in a small, curated set of files
that are expected to stay in sync (Bazel packaging, Meson, Python metadata,
C++ bindings, docs).

Workflow:
  1) Reads the current version (from pyproject.toml).
  2) Prompts for a new version (unless --new-version is provided).
  3) Prints exactly which files/fields will be updated.
  4) Asks for confirmation before writing (unless --yes is provided).
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


def find_fimage_workspace() -> Path:
    """Return the Bazel workspace root for the fimage module."""
    path = Path(__file__).resolve()
    for parent in path.parents:
        module_file = parent / "MODULE.bazel"
        if not module_file.is_file():
            continue
        if 'name = "fimage"' in module_file.read_text(encoding="utf-8"):
            return parent
    raise RuntimeError('Could not locate the fimage Bazel workspace (MODULE.bazel with name = "fimage").')


WORKSPACE_ROOT = find_fimage_workspace()


@dataclass(frozen=True)
class PlannedEdit:
    path: Path
    description: str
    old: str
    new: str


def _read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _write_text(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")


def _validate_version(version: str) -> None:
    # Keep it strict/simple: semantic versions like 1.2.3 (optionally with -rc.1 / +local).
    # Note: place '-' at the end of the character class to avoid unintended ranges.
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:[-+][0-9A-Za-z.+-]+)?", version):
        raise ValueError(f"Invalid version '{version}'. Expected semver-like 'X.Y.Z' (optionally with -suffix/+meta).")


def _read_current_fimage_version() -> str:
    pyproject = WORKSPACE_ROOT / "pyproject.toml"
    match = re.search(r'^version\s*=\s*"([^"]+)"\s*$', _read_text(pyproject), flags=re.MULTILINE)
    if not match:
        raise ValueError(f"Could not determine current fimage version from {pyproject}")
    return match.group(1)


def _plan_regex_sub(
    *,
    path: Path,
    description: str,
    pattern: str,
    replacement: str,
    flags: int = 0,
    expected_matches: int | None = 1,
) -> PlannedEdit:
    content = _read_text(path)
    new_content, n = re.subn(pattern, replacement, content, flags=flags)
    if expected_matches is None:
        if n < 1:
            raise ValueError(f"{path}: expected at least 1 match for {description} (got {n})")
    elif n != expected_matches:
        raise ValueError(f"{path}: expected exactly {expected_matches} match(es) for {description} (got {n})")
    return PlannedEdit(path=path, description=description, old=content, new=new_content)


def _collect_plans(*, new_version: str) -> list[PlannedEdit]:
    plans: list[PlannedEdit] = []

    # Bazel module definition.
    plans.append(
        _plan_regex_sub(
            path=WORKSPACE_ROOT / "MODULE.bazel",
            description="Update version in MODULE.bazel",
            pattern=r'(^module\(\s*\n\s*name\s*=\s*"fimage",\s*\n\s*version\s*=\s*")[^"]*(")',
            replacement=f"\\g<1>{new_version}\\g<2>",
            flags=re.MULTILINE,
        )
    )

    # Bazel wheel version constant.
    plans.append(
        _plan_regex_sub(
            path=WORKSPACE_ROOT / "python" / "BUILD.bazel",
            description="Update FIM_VERSION in python/BUILD.bazel",
            pattern=r'^(FIM_VERSION\s*=\s*)"[^"]*"\s*$',
            replacement=f'\\g<1>"{new_version}"',
            flags=re.MULTILINE,
        )
    )

    # Meson build. Anchor on a line-leading `version` so the `meson_version`
    # field on the following line is never matched.
    plans.append(
        _plan_regex_sub(
            path=WORKSPACE_ROOT / "meson.build",
            description="Update project() version in meson.build",
            pattern=r"^(\s*version\s*:\s*)'[^']*'(\s*,)",
            replacement=f"\\g<1>'{new_version}'\\g<2>",
            flags=re.MULTILINE,
        )
    )

    # Python package metadata.
    plans.append(
        _plan_regex_sub(
            path=WORKSPACE_ROOT / "pyproject.toml",
            description="Update version in pyproject.toml",
            pattern=r'^(version\s*=\s*)"[^"]*"\s*$',
            replacement=f'\\g<1>"{new_version}"',
            flags=re.MULTILINE,
        )
    )

    # C++ bindings version marker.
    plans.append(
        _plan_regex_sub(
            path=WORKSPACE_ROOT / "src" / "python" / "bindings.cpp",
            description='Update m.attr("__version__") in src/python/bindings.cpp',
            pattern=r'^(\s*m\.attr\("__version__"\)\s*=\s*)"[^"]*"(;\s*)$',
            replacement=f'\\g<1>"{new_version}"\\g<2>',
            flags=re.MULTILINE,
        )
    )

    # Docs.
    plans.append(
        _plan_regex_sub(
            path=WORKSPACE_ROOT / "docs" / "source" / "conf.py",
            description="Update release in docs/source/conf.py",
            pattern=r'^(release\s*=\s*)"[^"]*"\s*$',
            replacement=f'\\g<1>"{new_version}"',
            flags=re.MULTILINE,
        )
    )
    plans.append(
        _plan_regex_sub(
            path=WORKSPACE_ROOT / "docs" / "Doxyfile",
            description="Update PROJECT_NUMBER in docs/Doxyfile",
            pattern=r"^(PROJECT_NUMBER\s*=\s*)[0-9A-Za-z.+-]+\s*$",
            replacement=f"\\g<1>{new_version}",
            flags=re.MULTILINE,
        )
    )

    # Docs landing-page version badge (shields.io image URL + its alt text).
    plans.append(
        _plan_regex_sub(
            path=WORKSPACE_ROOT / "docs" / "source" / "index.rst",
            description="Update version badge URL in docs/source/index.rst",
            pattern=r"(badge/version-)[0-9A-Za-z.+%-]+(-blue\.svg)",
            replacement=f"\\g<1>{new_version}\\g<2>",
        )
    )
    plans.append(
        _plan_regex_sub(
            path=WORKSPACE_ROOT / "docs" / "source" / "index.rst",
            description="Update version badge alt text in docs/source/index.rst",
            pattern=r"^(\s*:alt: Version )[0-9A-Za-z.+-]+\s*$",
            replacement=f"\\g<1>{new_version}",
            flags=re.MULTILINE,
        )
    )

    # Sanity: ensure all plans actually change something.
    for p in plans:
        if p.old == p.new:
            raise ValueError(f"{p.path}: planned edit made no changes ({p.description})")

    return plans


def _print_plan(plans: list[PlannedEdit]) -> None:
    print("\nPlanned version updates:\n")
    for p in plans:
        print(f"- {p.path.relative_to(WORKSPACE_ROOT)}: {p.description}")
    print()


def _prompt(prompt: str) -> str:
    return input(prompt).strip()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--new-version",
        help="New version to set. If omitted, you'll be prompted.",
    )
    parser.add_argument(
        "--yes",
        action="store_true",
        help="Apply the updates without an interactive confirmation prompt.",
    )
    args = parser.parse_args()

    current_version = _read_current_fimage_version()
    print(f"Current fimage version: {current_version}")

    new_version = args.new_version
    if not new_version:
        new_version = _prompt("Enter new version (e.g. 0.1.2): ")
    if not new_version:
        raise SystemExit("No version provided.")
    _validate_version(new_version)
    if new_version == current_version:
        raise SystemExit("New version matches current version; nothing to do.")

    plans = _collect_plans(new_version=new_version)
    _print_plan(plans)

    if not args.yes:
        confirm = _prompt(f"Apply these {len(plans)} updates? [y/N]: ").lower()
        if confirm not in ("y", "yes"):
            print("Aborted; no files were changed.")
            return

    for p in plans:
        _write_text(p.path, p.new)

    print("\nDone. Updated files:")
    for p in plans:
        print(f"- {p.path.relative_to(WORKSPACE_ROOT)}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nAborted.")
        sys.exit(1)
