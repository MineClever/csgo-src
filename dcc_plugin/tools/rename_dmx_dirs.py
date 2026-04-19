from __future__ import annotations

import os
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]

TEXT_FILE_NAMES = {"CMakeLists.txt", "AGENTS.md", "Plan.md"}
TEXT_FILE_SUFFIXES = {
    ".cpp",
    ".h",
    ".hpp",
    ".cxx",
    ".cc",
    ".cmake",
    ".md",
    ".mel",
    ".py",
    ".txt",
    ".json",
    ".bat",
}

BYTE_REPLACEMENTS = [
    (b"src/common_dmx)", b"src/common_dmx)"),
    (b"src/importer_dmx)", b"src/importer_dmx)"),
    (b"src/exporter_dmx)", b"src/exporter_dmx)"),
    (b"src/plugin_dmx)", b"src/plugin_dmx)"),
    (b"src/common_dmx/", b"src/common_dmx/"),
    (b"src/importer_dmx/", b"src/importer_dmx/"),
    (b"src/exporter_dmx/", b"src/exporter_dmx/"),
    (b"src/plugin_dmx/", b"src/plugin_dmx/"),
    (b"src\\common\\", b"src\\common_dmx\\"),
    (b"src\\importer\\", b"src\\importer_dmx\\"),
    (b"src\\exporter\\", b"src\\exporter_dmx\\"),
    (b"src\\plugin\\", b"src\\plugin_dmx\\"),
    (b"<common_dmx/", b"<common_dmx/"),
    (b"<importer_dmx/", b"<importer_dmx/"),
    (b"<exporter_dmx/", b"<exporter_dmx/"),
    (b"<plugin_dmx/", b"<plugin_dmx/"),
    (b'"../common_dmx/', b'"../common_dmx/'),
    (b'"../importer_dmx/', b'"../importer_dmx/'),
    (b'"../exporter_dmx/', b'"../exporter_dmx/'),
    (b'"../plugin_dmx/', b'"../plugin_dmx/'),
    (b'"common_dmx/', b'"common_dmx/'),
    (b'"importer_dmx/', b'"importer_dmx/'),
    (b'"exporter_dmx/', b'"exporter_dmx/'),
    (b'"plugin_dmx/', b'"plugin_dmx/'),
]

DIR_RENAMES = [
    ("dcc_plugin/src/common", "dcc_plugin/src/common_dmx"),
    ("dcc_plugin/src/importer", "dcc_plugin/src/importer_dmx"),
    ("dcc_plugin/src/exporter", "dcc_plugin/src/exporter_dmx"),
    ("dcc_plugin/src/plugin", "dcc_plugin/src/plugin_dmx"),
]

SKIP_DIRS = {".git", ".vs", "build", "out", "__pycache__"}


def is_text_file(path: Path) -> bool:
    return path.name in TEXT_FILE_NAMES or path.suffix.lower() in TEXT_FILE_SUFFIXES


def rewrite_file(path: Path) -> bool:
    data = path.read_bytes()
    new_data = data
    for old, new in BYTE_REPLACEMENTS:
        new_data = new_data.replace(old, new)
    if new_data != data:
        path.write_bytes(new_data)
        return True
    return False


def rewrite_tree(root: Path) -> int:
    changed = 0
    for current_root, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        current_path = Path(current_root)
        for filename in filenames:
            file_path = current_path / filename
            if is_text_file(file_path) and rewrite_file(file_path):
                changed += 1
    return changed


def rename_directories() -> None:
    for old_rel, new_rel in DIR_RENAMES:
        old_path = REPO_ROOT / old_rel
        new_path = REPO_ROOT / new_rel
        if not old_path.exists():
            continue
        if new_path.exists():
            raise RuntimeError(f"Target directory already exists: {new_path}")
        old_path.rename(new_path)


def main() -> None:
    changed = rewrite_tree(REPO_ROOT / "dcc_plugin")
    plan_path = REPO_ROOT / "Plan.md"
    if plan_path.exists() and rewrite_file(plan_path):
        changed += 1
    rename_directories()
    print(f"Updated {changed} files and renamed {len(DIR_RENAMES)} directories.")


if __name__ == "__main__":
    main()
