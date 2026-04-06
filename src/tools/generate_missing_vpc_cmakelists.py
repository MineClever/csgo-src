#!/usr/bin/env python3
"""
Generate placeholder CMakeLists.txt files for directories that contain VPC/VGC
files but do not yet have a CMake entrypoint.

This keeps repository coverage aligned: every VPC directory has a matching
CMakeLists.txt, even when the target has not been fully ported to CMake.
"""

from __future__ import annotations

import argparse
from pathlib import Path


def find_vpc_files(directory: Path) -> list[Path]:
    files: list[Path] = []
    for path in sorted(directory.iterdir()):
        if not path.is_file():
            continue
        suffix = path.suffix.lower()
        if suffix not in {".vpc", ".vgc"}:
            continue
        files.append(path)
    return files


def find_missing_dirs(src_dir: Path) -> list[Path]:
    vpc_dirs: set[Path] = set()
    for path in src_dir.rglob("*"):
        if not path.is_file():
            continue
        suffix = path.suffix.lower()
        if suffix not in {".vpc", ".vgc"}:
            continue
        vpc_dirs.add(path.parent)
    return sorted(directory for directory in vpc_dirs if not (directory / "CMakeLists.txt").exists())


def direct_child_cmake_dirs(directory: Path, generated_dirs: set[Path]) -> list[str]:
    children: list[str] = []
    for child in sorted(directory.iterdir()):
        if not child.is_dir():
            continue
        if (child / "CMakeLists.txt").exists() or child in generated_dirs:
            children.append(child.name)
    return children


def render_cmakelists(src_dir: Path, directory: Path, generated_dirs: set[Path]) -> str:
    rel_dir = directory.relative_to(src_dir).as_posix()
    vpc_files = find_vpc_files(directory)
    child_dirs = direct_child_cmake_dirs(directory, generated_dirs)

    lines: list[str] = []
    lines.append(f"# {rel_dir}")
    lines.append("#")
    lines.append("# This directory contains legacy VPC/VGC definitions but does not yet")
    lines.append("# have a full CMake port. This placeholder keeps repository-level")
    lines.append("# CMake coverage aligned with the original VPC layout.")
    lines.append("")
    lines.append("# VPC sources in this directory:")
    for file in vpc_files:
        lines.append(f"# - {file.name}")

    if child_dirs:
        lines.append("")
        lines.append("# Subdirectories that already provide CMake entrypoints:")
        for child in child_dirs:
            lines.append(f"add_subdirectory({child})")
    else:
        lines.append("")
        lines.append("# No standalone CMake target is defined here yet.")

    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate placeholder CMakeLists.txt for missing VPC directories.")
    parser.add_argument("--src-dir", default=None, help="Path to the src directory. Defaults to the parent of this script.")
    parser.add_argument("--dry-run", action="store_true", help="Print the files that would be generated without writing them.")
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    src_dir = Path(args.src_dir).resolve() if args.src_dir else script_dir.parent

    if not src_dir.is_dir():
        raise SystemExit(f"src directory not found: {src_dir}")

    missing_dirs = find_missing_dirs(src_dir)
    generated_dirs = set(missing_dirs)
    print(f"Found {len(missing_dirs)} VPC directories without CMakeLists.txt.")

    for directory in missing_dirs:
        cmake_path = directory / "CMakeLists.txt"
        content = render_cmakelists(src_dir, directory, generated_dirs)
        if args.dry_run:
            print(f"\n# {cmake_path.relative_to(src_dir.parent).as_posix()}")
            print(content)
        else:
            cmake_path.write_text(content, encoding="utf-8")
            print(f"Wrote {cmake_path.relative_to(src_dir.parent).as_posix()}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
