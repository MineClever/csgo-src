from __future__ import annotations

from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
DCC_ROOT = REPO_ROOT / "dcc_plugin"
COMMON_DMX = DCC_ROOT / "src" / "common_dmx"
COMMON_SHARED = DCC_ROOT / "src" / "common"

SHARED_FILES = [
    "AnimationCurveUtils.cpp",
    "AnimationCurveUtils.h",
    "AnimationSampleUtils.cpp",
    "AnimationSampleUtils.h",
    "ExportAnimationUtils.cpp",
    "ExportAnimationUtils.h",
    "ImportPolicy.h",
    "ImportTransformCorrection.cpp",
    "ImportTransformCorrection.h",
    "MaterialExportUtils.cpp",
    "MaterialExportUtils.h",
    "MaterialUtils.cpp",
    "MaterialUtils.h",
    "MayaCommandUtils.cpp",
    "MayaCommandUtils.h",
    "SceneMergeStrategy.h",
    "SkinClusterUtils.cpp",
    "SkinClusterUtils.h",
    "SourceDeltaUtils.h",
]

TEXT_SUFFIXES = {
    ".cpp",
    ".h",
    ".hpp",
    ".cc",
    ".cxx",
    ".cmake",
    ".md",
    ".mel",
    ".py",
    ".txt",
    ".json",
}


def replace_in_file(path: Path) -> bool:
    data = path.read_bytes()
    updated = data
    for name in SHARED_FILES:
        updated = updated.replace(f"<common_dmx/{name}>".encode("utf-8"), f"<common/{name}>".encode("utf-8"))
        updated = updated.replace(f'"../common_dmx/{name}"'.encode("utf-8"), f'"../common/{name}"'.encode("utf-8"))
        updated = updated.replace(f'"common_dmx/{name}"'.encode("utf-8"), f'"common/{name}"'.encode("utf-8"))
    if updated != data:
        path.write_bytes(updated)
        return True
    return False


def rewrite_tree(root: Path) -> int:
    changed = 0
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if "build" in path.parts or ".git" in path.parts:
            continue
        if path.suffix.lower() not in TEXT_SUFFIXES and path.name not in {"CMakeLists.txt", "Plan.md"}:
            continue
        if replace_in_file(path):
            changed += 1
    return changed


def move_shared_files() -> None:
    COMMON_SHARED.mkdir(parents=True, exist_ok=True)
    for name in SHARED_FILES:
        source = COMMON_DMX / name
        target = COMMON_SHARED / name
        if not source.exists():
            raise FileNotFoundError(source)
        if target.exists():
            raise FileExistsError(target)
        source.rename(target)


def main() -> None:
    changed = rewrite_tree(REPO_ROOT)
    move_shared_files()
    print(f"Updated {changed} files and moved {len(SHARED_FILES)} shared files.")


if __name__ == "__main__":
    main()
