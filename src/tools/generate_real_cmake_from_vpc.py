#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path


TRUE_FLAGS = {
    "WINDOWS": True,
    "WIN32": True,
    "WIN64": False,
    "POSIX": False,
    "OSXALL": False,
    "OSX32": False,
    "OSX64": False,
    "LINUX": False,
    "X360": False,
    "PS3": False,
    "VS2010": False,
    "VS2012": False,
    "VS2013": False,
    "VS2015": True,
    "GL": True,
}

CODE_EXTS = {".c", ".cc", ".cpp", ".cxx", ".mm", ".rc"}
HEADER_EXTS = {".h", ".hh", ".hpp", ".hxx", ".inl"}
GLOBAL_DEFINES = {
    "WIN32", "_WIN32", "COMPILER_MSVC", "COMPILER_MSVC32", "VPC",
    "AVI_VIDEO", "WMV_VIDEO", "CSTRIKE15", "_HAS_ITERATOR_DEBUGGING=0",
    "_DEBUG", "DEBUG", "NDEBUG", "_WINDOWS",
    "_CRT_SECURE_NO_DEPRECATE", "_CRT_NONSTDC_NO_DEPRECATE",
    "_ALLOW_RUNTIME_LIBRARY_MISMATCH", "_ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH",
    "_ALLOW_MSC_VER_MISMATCH",
}
SYSTEM_LIBS = {
    "shell32.lib", "user32.lib", "advapi32.lib", "gdi32.lib", "comdlg32.lib",
    "ole32.lib", "ws2_32.lib", "wsock32.lib", "winmm.lib", "wininet.lib",
    "odbc32.lib", "odbccp32.lib", "imm32.lib", "version.lib", "vfw32.lib",
    "uuid.lib", "oleaut32.lib", "comctl32.lib", "rpcrt4.lib", "iphlpapi.lib",
}


@dataclass
class VpcProject:
    directory: Path
    project_name: str = ""
    target_kind: str = "STATIC"
    no_standard_valve_code: bool = False
    source_files: list[str] = field(default_factory=list)
    header_files: list[str] = field(default_factory=list)
    include_dirs: list[str] = field(default_factory=list)
    defines: list[str] = field(default_factory=list)
    link_libs: list[str] = field(default_factory=list)
    link_paths: list[str] = field(default_factory=list)
    out_bindir: str = ""
    output_name: str = ""
    generated_from: list[str] = field(default_factory=list)


def sanitize_target(name: str) -> str:
    target = re.sub(r"[^A-Za-z0-9_]+", "_", name).strip("_")
    if not target:
        target = "legacy_vpc_target"
    if re.match(r"^[0-9]", target):
        target = f"target_{target}"
    return target


def split_values(raw: str) -> list[str]:
    raw = raw.replace(";", ",")
    return [part.strip() for part in raw.split(",") if part.strip() and part.strip() != "$BASE"]


def strip_comments(line: str) -> str:
    if "//" not in line:
        return line
    result: list[str] = []
    in_string = False
    i = 0
    while i < len(line):
        ch = line[i]
        if ch == '"':
            in_string = not in_string
            result.append(ch)
            i += 1
            continue
        if not in_string and line[i:i + 2] == "//":
            break
        result.append(ch)
        i += 1
    return "".join(result)


def preprocess(text: str) -> list[str]:
    lines: list[str] = []
    current = ""
    for raw in text.splitlines():
        line = strip_comments(raw).rstrip()
        if not line.strip():
            continue
        if line.endswith("\\"):
            current += line[:-1] + " "
            continue
        current += line
        lines.append(current.strip())
        current = ""
    if current.strip():
        lines.append(current.strip())
    return lines


def eval_condition(expr: str) -> bool:
    expr = expr.strip()
    if not expr:
        return True

    def repl(match: re.Match[str]) -> str:
        name = match.group(1)
        return "True" if TRUE_FLAGS.get(name, False) else "False"

    python_expr = re.sub(r"\$([A-Za-z0-9_]+)", repl, expr)
    python_expr = python_expr.replace("&&", " and ").replace("||", " or ").replace("!", " not ")
    try:
        return bool(eval(python_expr, {"__builtins__": {}}, {}))
    except Exception:
        return False


def split_condition(line: str) -> tuple[str, bool]:
    match = re.search(r"\[(.+)\]\s*$", line)
    if not match:
        return line, True
    condition = match.group(1)
    return line[:match.start()].rstrip(), eval_condition(condition)


def replace_macros(value: str, macros: dict[str, str], directory: Path, src_dir: Path) -> str:
    value = value.strip().strip('"')
    for _ in range(10):
        replaced = re.sub(r"\$([A-Za-z0-9_]+)", lambda m: macros.get(m.group(1), f"${m.group(1)}"), value)
        if replaced == value:
            break
        value = replaced

    value = value.replace("\\", "/")
    value = value.replace("$(TargetName)", macros.get("OUTBINNAME", ""))

    cmake_map = {
        str(src_dir).replace("\\", "/"): "${SRC_DIR}",
        str((src_dir / "lib" / "public").resolve()).replace("\\", "/"): "${LIB_PUBLIC}",
        str((src_dir / "lib" / "common").resolve()).replace("\\", "/"): "${LIB_COMMON}",
        str((src_dir.parent / "game" / "bin").resolve()).replace("\\", "/"): "${GAME_BIN_DIR}",
        str((src_dir.parent / "game" / "csgo" / "bin").resolve()).replace("\\", "/"): "${GAME_CSGO_BIN}",
        str((src_dir.parent / "game").resolve()).replace("\\", "/"): "${GAME_DIR}",
    }

    if value.startswith("${"):
        return value
    if "$" in value:
        return value

    path = Path(value)
    try:
        if not path.is_absolute() and not re.match(r"^[A-Za-z_][A-Za-z0-9_]*:", value):
            path = (directory / value).resolve()
        else:
            path = path.resolve() if path.is_absolute() else path
    except OSError:
        return value

    path_str = str(path).replace("\\", "/")
    for prefix, cmake_prefix in cmake_map.items():
        if path_str == prefix:
            return cmake_prefix
        if path_str.startswith(prefix + "/"):
            return cmake_prefix + path_str[len(prefix):]
    return path_str


def classify_path(path: str) -> str:
    ext = Path(path).suffix.lower()
    if ext in CODE_EXTS:
        return "source"
    if ext in HEADER_EXTS:
        return "header"
    return "other"


def parse_vpc_file(
    vpc_path: Path,
    project: VpcProject,
    src_dir: Path,
    seen: set[Path],
    inherited_macros: dict[str, str] | None = None,
) -> None:
    resolved = vpc_path.resolve()
    if resolved in seen or not vpc_path.exists():
        return
    seen.add(resolved)
    try:
        shown_path = str(vpc_path.relative_to(project.directory))
    except ValueError:
        shown_path = str(vpc_path.relative_to(src_dir))
    project.generated_from.append(shown_path)

    macros = dict(inherited_macros or {})
    macros.setdefault("SRCDIR", str(src_dir).replace("\\", "/"))
    macros.setdefault("LIBPUBLIC", str((src_dir / "lib" / "public").resolve()).replace("\\", "/"))
    macros.setdefault("LIBCOMMON", str((src_dir / "lib" / "common").resolve()).replace("\\", "/"))

    lines = preprocess(vpc_path.read_text(encoding="utf-8", errors="ignore"))
    context_stack: list[tuple[str, str]] = []
    active_stack: list[bool] = []
    pending_context: tuple[str, str] | None = None
    pending_active = True

    for raw_line in lines:
        line, allowed = split_condition(raw_line)
        if not allowed:
            continue

        if line == "{":
            if pending_context is not None:
                context_stack.append(pending_context)
                active_stack.append(pending_active)
                pending_context = None
                pending_active = True
            continue
        if line == "}":
            if context_stack:
                context_stack.pop()
                active_stack.pop()
            continue
        if active_stack and not all(active_stack):
            continue

        macro_match = re.match(r"\$Macro(?:Required)?\s+([A-Za-z0-9_]+)\s+\"([^\"]*)\"", line, flags=re.I)
        if macro_match:
            macros[macro_match.group(1)] = replace_macros(macro_match.group(2), macros, vpc_path.parent, src_dir)
            continue

        cond_match = re.match(r"\$Conditional\s+([A-Za-z0-9_]+)\s+\"([^\"]+)\"", line, flags=re.I)
        if cond_match and cond_match.group(1) == "NO_STANDARD_VALVE_CODE":
            project.no_standard_valve_code = cond_match.group(2).lower() == "true"
            continue

        include_match = re.match(r"\$Include\s+\"([^\"]+)\"", line, flags=re.I)
        if include_match:
            include_raw = replace_macros(include_match.group(1), macros, vpc_path.parent, src_dir)
            include_path = Path(include_raw.replace("${SRC_DIR}", str(src_dir).replace("\\", "/")))
            if not include_path.is_absolute():
                include_path = (vpc_path.parent / include_path).resolve()
            include_name = include_path.name.lower()
            if "source_lib" in include_name:
                project.target_kind = "STATIC"
            elif "source_dll" in include_name:
                project.target_kind = "SHARED"
            elif "source_exe" in include_name:
                project.target_kind = "EXECUTABLE"
            parse_vpc_file(include_path, project, src_dir, seen, macros)
            continue

        project_match = re.match(r"\$Project\s+\"([^\"]+)\"", line, flags=re.I)
        if project_match:
            if not project.project_name:
                project.project_name = project_match.group(1).strip()
            pending_context = ("project", project_match.group(1).strip())
            continue

        folder_match = re.match(r"\$Folder\s+\"([^\"]+)\"", line, flags=re.I)
        if folder_match:
            pending_context = ("folder", folder_match.group(1).strip())
            continue

        config_match = re.match(r"\$Configuration(?:\s+\"([^\"]+)\")?$", line, flags=re.I)
        if config_match:
            pending_context = ("configuration", config_match.group(1) or "")
            continue

        if re.match(r"\$Compiler$", line, flags=re.I):
            pending_context = ("compiler", "")
            continue
        if re.match(r"\$Linker$", line, flags=re.I):
            pending_context = ("linker", "")
            continue

        current_folders = [name for kind, name in context_stack if kind == "folder"]
        in_link_folder = any(name.lower() == "link libraries" for name in current_folders)
        in_compiler = any(kind == "compiler" for kind, _ in context_stack)
        in_linker = any(kind == "linker" for kind, _ in context_stack)

        if in_compiler:
            inc_match = re.match(r"\$AdditionalIncludeDirectories\s+\"([^\"]+)\"", line, flags=re.I)
            if inc_match:
                for item in split_values(inc_match.group(1)):
                    if item == "$BASE":
                        continue
                    converted = replace_macros(item, macros, vpc_path.parent, src_dir)
                    if converted not in project.include_dirs:
                        project.include_dirs.append(converted)
                continue

            define_match = re.match(r"\$PreprocessorDefinitions\s+\"([^\"]+)\"", line, flags=re.I)
            if define_match:
                for item in split_values(define_match.group(1)):
                    if item == "$BASE":
                        continue
                    item = item.replace('"', "")
                    if item and item not in project.defines:
                        project.defines.append(item)
                continue

        if in_linker:
            dep_match = re.match(r"\$AdditionalDependencies\s+\"([^\"]+)\"", line, flags=re.I)
            if dep_match:
                for item in split_values(dep_match.group(1).replace(" ", ",")):
                    if item == "$BASE":
                        continue
                    converted = replace_macros(item, macros, vpc_path.parent, src_dir)
                    if converted not in project.link_paths:
                        project.link_paths.append(converted)
                continue

        out_bindir_match = re.match(r"\$Macro\s+OUTBINDIR\s+\"([^\"]+)\"", line, flags=re.I)
        if out_bindir_match and not project.out_bindir:
            project.out_bindir = replace_macros(out_bindir_match.group(1), macros, vpc_path.parent, src_dir)
            continue

        file_matches = re.findall(r"\$File\s+\"([^\"]+)\"", line, flags=re.I)
        if file_matches:
            for raw_path in file_matches:
                converted = replace_macros(raw_path, macros, vpc_path.parent, src_dir)
                if in_link_folder:
                    if converted not in project.link_paths:
                        project.link_paths.append(converted)
                    continue
                kind = classify_path(converted)
                if kind == "source" and converted not in project.source_files:
                    project.source_files.append(converted)
                elif kind == "header" and converted not in project.header_files:
                    project.header_files.append(converted)
            continue

        lib_match = re.match(r"\$Lib\s+([^\s]+)", line, flags=re.I)
        if lib_match:
            lib = lib_match.group(1).strip()
            if lib not in project.link_libs:
                project.link_libs.append(lib)


def existing_targets(src_dir: Path) -> set[str]:
    targets: set[str] = set()
    pattern = re.compile(r"add_(?:library|executable)\(([A-Za-z0-9_]+)")
    for path in src_dir.rglob("CMakeLists.txt"):
        text = path.read_text(encoding="utf-8", errors="ignore")
        for match in pattern.finditer(text):
            targets.add(match.group(1))
    return targets


def classify_link_item(item: str, known_targets: set[str]) -> str:
    normalized = item.strip().strip('"').replace("\\", "/")
    leaf = Path(normalized).name
    stem = Path(leaf).stem

    if leaf.lower() in SYSTEM_LIBS:
        return leaf

    for target in known_targets:
        if target.lower() == stem.lower():
            return target

    prebuilt = {
        "dmeutils", "fbxutils", "togl", "nvtristrip", "libpng", "libjpeg",
        "jpeglib", "mxtoolkitwin32", "bzip2", "steam_api", "libprotobuf",
        "libz", "nvtc", "cryptlib",
    }
    if stem.lower() in prebuilt:
        return f"prebuilt::{stem}"

    if leaf.lower().endswith(".lib"):
        return f"\"{normalized}\""
    return item


def render_project(project: VpcProject, cmake_target: str, known_targets: set[str]) -> str:
    lines: list[str] = []
    var_name = sanitize_target(cmake_target.upper()) + "_SOURCES"

    lines.append(f"# {project.directory.relative_to(project.directory.parents[1]).as_posix() if len(project.directory.parents) > 1 else project.directory.name}")
    lines.append(f"# Generated from: {', '.join(project.generated_from)}")
    lines.append("")
    lines.append(f"set({var_name}")
    for path in project.source_files + project.header_files:
        lines.append(f"    {path}")
    lines.append(")")
    lines.append("")

    if project.target_kind == "EXECUTABLE":
        lines.append(f"add_executable({cmake_target} EXCLUDE_FROM_ALL ${{{var_name}}})")
    else:
        lines.append(f"add_library({cmake_target} {project.target_kind} EXCLUDE_FROM_ALL ${{{var_name}}})")
    lines.append("")

    if not project.no_standard_valve_code:
        lines.append(f"valve_apply_base_settings({cmake_target})")
        lines.append("")

    if project.include_dirs:
        lines.append(f"target_include_directories({cmake_target} PRIVATE")
        for item in project.include_dirs:
            lines.append(f"    {item}")
        lines.append(")")
        lines.append("")

    filtered_defines: list[str] = []
    for item in project.defines:
        if (
            item in GLOBAL_DEFINES
            or item.startswith("%")
            or "$" in item
            or item in filtered_defines
        ):
            continue
        filtered_defines.append(item)

    if filtered_defines:
        lines.append(f"target_compile_definitions({cmake_target} PRIVATE")
        for item in filtered_defines:
            lines.append(f"    {item}")
        lines.append(")")
        lines.append("")

    link_items: list[str] = []
    for item in project.link_libs + project.link_paths:
        classified = classify_link_item(item, known_targets)
        if classified not in link_items:
            link_items.append(classified)
    if link_items:
        lines.append(f"target_link_libraries({cmake_target} PRIVATE")
        for item in link_items:
            lines.append(f"    {item}")
        lines.append(")")
        lines.append("")

    if project.output_name:
        lines.append(f"set_target_properties({cmake_target} PROPERTIES OUTPUT_NAME \"{project.output_name}\")")
        lines.append("")

    if project.target_kind == "STATIC":
        lines.append(f"valve_set_static_lib_output({cmake_target})")
    elif project.target_kind == "SHARED":
        lines.append(f"valve_set_dll_importlib_output({cmake_target})")
        lines.append(f"valve_publish_dll({cmake_target} \"${{GAME_BIN_DIR}}\")")
    else:
        lines.append(f"valve_publish_exe({cmake_target} \"${{GAME_BIN_DIR}}\")")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--src-dir", default=None)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    src_dir = Path(args.src_dir).resolve() if args.src_dir else script_dir.parent
    existing = existing_targets(src_dir)

    target_dirs: list[Path] = []
    for path in src_dir.rglob("*.vpc"):
        directory = path.parent
        cmake = directory / "CMakeLists.txt"
        if not cmake.exists():
            continue
        text = cmake.read_text(encoding="utf-8", errors="ignore")
        if "legacy VPC/VGC definitions" in text or "Generated from:" in text:
            target_dirs.append(directory)
    target_dirs = sorted(set(target_dirs))

    duplicate_counter: defaultdict[str, int] = defaultdict(int)
    for directory in target_dirs:
        vpc_files = sorted(directory.glob("*.vpc"))
        projects: list[VpcProject] = []
        for vpc_path in vpc_files:
            project = VpcProject(directory=directory)
            parse_vpc_file(vpc_path, project, src_dir, set())
            if project.project_name and project.source_files:
                projects.append(project)
        if not projects:
            continue

        rendered: list[str] = []
        for project in projects:
            base_target = sanitize_target(project.project_name)
            if base_target in existing or duplicate_counter[base_target] > 0:
                rel_target = sanitize_target(directory.relative_to(src_dir).as_posix().replace("/", "_"))
                cmake_target = f"{rel_target}_{base_target}"
                project.output_name = project.project_name if "$" not in project.project_name else ""
            else:
                cmake_target = base_target
            duplicate_counter[cmake_target] += 1
            existing.add(cmake_target)
            rendered.append(render_project(project, cmake_target, existing))

        output = "\n".join(rendered).rstrip() + "\n"
        cmake_path = directory / "CMakeLists.txt"
        if args.dry_run:
            print(f"\n# {cmake_path.relative_to(src_dir.parent).as_posix()}")
            print(output[:4000])
        else:
            cmake_path.write_text(output, encoding="utf-8")
            print(f"Wrote {cmake_path.relative_to(src_dir.parent).as_posix()}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
