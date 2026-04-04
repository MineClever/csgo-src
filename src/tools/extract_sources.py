#!/usr/bin/env python3
"""
extract_sources.py
从 VPC 生成的 .vcxproj 文件中提取源文件和配置信息，
为每个目标生成对应的 CMakeLists.txt 文件。

用法：
    python extract_sources.py                  # 在 src/ 目录下运行，生成所有文件
    python extract_sources.py --dry-run        # 仅预览，不写文件
    python extract_sources.py --target tier0   # 仅生成指定目标
"""

import xml.etree.ElementTree as ET
import os
import sys
import re
import argparse
from pathlib import Path
from collections import defaultdict

# ---------------------------------------------------------------------------
# MSBuild XML 命名空间
# ---------------------------------------------------------------------------
NS = "http://schemas.microsoft.com/developer/msbuild/2003"
NSD = {"ms": NS}


# ---------------------------------------------------------------------------
# 全局宏定义（在 ValveBase.cmake 中已定义，提取时需过滤掉）
# ---------------------------------------------------------------------------
GLOBAL_DEFINES = {
    "WIN32", "_WIN32", "COMPILER_MSVC", "COMPILER_MSVC32",
    "_DLL_EXT=.dll", "CSTRIKE15", "AVI_VIDEO", "WMV_VIDEO",
    "RAD_TELEMETRY_DISABLED", "CSTRIKE_REL_BUILD=1",
    "_CRT_SECURE_NO_DEPRECATE", "_CRT_NONSTDC_NO_DEPRECATE",
    "_ALLOW_RUNTIME_LIBRARY_MISMATCH", "_ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH",
    "_ALLOW_MSC_VER_MISMATCH", "VPC",
    # Debug/Release 特有（也在 ValveBase 中设置）
    "_DEBUG", "DEBUG", "_HAS_ITERATOR_DEBUGGING=0", "NDEBUG",
    # VPC 生成的机器相关或冗余宏（过滤掉）
    "_WINDOWS", "_DLL_PREFIX=", "_EXTERNAL_DLL_EXT=.dll",
    "VPCGAME=csgo", "VPCGAMECAPS=CSGO",
    # 占位符
    "%(PreprocessorDefinitions)",
}

# PROJECTDIR 等含绝对路径的宏前缀，需要特殊过滤
GLOBAL_DEFINE_PREFIXES_TO_SKIP = ("PROJECTDIR=",)

# 全局包含目录（ValveBase 已添加）
GLOBAL_INCLUDES = {
    "$(SolutionDir)..\\common", "$(SolutionDir)..\\public",
    "$(SolutionDir)..\\public\\tier0", "$(SolutionDir)..\\public\\tier1",
    "..\\common", "..\\public", "..\\public\\tier0", "..\\public\\tier1",
    "common", "public", "public/tier0", "public/tier1",
    "%(AdditionalIncludeDirectories)",
}

# 已知 CMake 目标名（我们自己构建的，链接时直接用目标名）
KNOWN_CMAKE_TARGETS = {
    "appframework", "valve_avi", "bitmap", "bitmap_byteswap", "bonesetup",
    "choreoobjects", "datacache", "Datamodel", "datamodel", "dedicated",
    "dmserializers", "dmxloader", "engine", "engine_ds", "fgdlib",
    "filesystem_stdio", "fow", "client", "server", "GameUI",
    "hammer_dll", "hammer_launcher", "inputsystem", "interfaces",
    "launcher", "launcher_main", "localize",
    "matchmaking_csgo", "matchmaking_ds_csgo", "matchmakingbase", "matchmakingbase_ds",
    "materialobjects", "materialsystem", "shaderapidx9", "shaderapiempty",
    "shaderlib", "stdshader_dbg", "stdshader_dx9",
    "mathlib", "mathlib_extended", "mdllib", "mdlobjects", "meshutils",
    "movieobjects", "particles", "raytrace", "resourcefile",
    "responserules_runtime", "scenefilecache", "serverbrowser",
    "soundemittersystem", "soundsystem_lowlevel", "soundsystem",
    "studiorender", "quickhull", "tier0", "tier1", "tier2", "tier3",
    "commedit", "pet", "toolutils", "vmt_tool", "unitlib",
    "FileSystemOpenDialog", "bzip2_test", "hlmv", "mxtoolkitwin32",
    "newdat", "shadercompile_dll", "shadercompile_launcher",
    "vbsp", "vbspinfo", "vrad_dll", "vrad_launcher",
    "vtex_dll", "vtex_launcher", "vvis_dll", "vvis_launcher",
    "dme_controls", "game_controls", "matsys_controls", "vgui_dll",
    "vgui_controls", "vgui_surfacelib", "vlocalize", "vguimatsurface",
    "videocfg", "vpklib", "vscript", "vstdlib", "vtf",
}

# 系统库（直接写名称，不加 prebuilt:: 前缀）
SYSTEM_LIBS = {
    "shell32.lib", "user32.lib", "advapi32.lib", "gdi32.lib",
    "comdlg32.lib", "ole32.lib", "ws2_32.lib", "winmm.lib",
    "wsock32.lib", "odbc32.lib", "odbccp32.lib", "shlwapi.lib",
    "rpcrt4.lib", "Rpcrt4.lib", "iphlpapi.lib", "Iphlpapi.lib",
    "dinput8.lib", "dsound.lib", "dxguid.lib", "d3d9.lib",
    "wininet.lib", "vfw32.lib", "imm32.lib", "winspool.lib",
    "setupapi.lib", "dbghelp.lib", "psapi.lib", "version.lib",
    "comdlg32.lib", "comctl32.lib", "uuid.lib", "oleaut32.lib",
    "opengl32.lib", "glu32.lib",
}


def tag(name):
    return f"{{{NS}}}{name}"


def find_text(elem, path):
    """在元素下查找文本，支持多级路径（用 / 分隔）。"""
    parts = path.split("/")
    cur = elem
    for p in parts:
        if cur is None:
            return ""
        cur = cur.find(tag(p))
    return (cur.text or "").strip() if cur is not None else ""


def find_all(elem, path):
    """查找所有匹配子元素，支持多级路径。"""
    parts = path.split("/")
    elems = [elem]
    for p in parts:
        new_elems = []
        for e in elems:
            new_elems.extend(e.findall(tag(p)))
        elems = new_elems
    return elems


def get_condition_config(elem):
    """从 Condition 属性中提取配置名（Debug/Release）。"""
    cond = elem.get("Condition", "")
    if "Debug" in cond:
        return "Debug"
    elif "Release" in cond:
        return "Release"
    return None


# ---------------------------------------------------------------------------
# 路径转换
# ---------------------------------------------------------------------------
def normalize_path(raw_path, vcxproj_dir, src_dir):
    """
    将 vcxproj 中的相对路径转换为 CMake 路径字符串。
    - 如果文件在同一目录下，返回相对名（basename）
    - 如果在 SRC_DIR 子目录下，返回 ${SRC_DIR}/rest
    - 否则返回绝对路径
    """
    # 替换 MSBuild 宏
    p = raw_path.replace("\\", "/")
    p = p.replace("$(SRCDIR)", "${SRC_DIR}")
    p = p.replace("$(IntDir)", "${CMAKE_CURRENT_BINARY_DIR}/")

    # 如果已包含 CMake 变量，直接返回
    if p.startswith("$"):
        return p

    # 解析为绝对路径
    if os.path.isabs(p):
        abs_path = Path(p)
    else:
        abs_path = (Path(vcxproj_dir) / p).resolve()

    # 尝试相对于 SRC_DIR
    try:
        rel = abs_path.relative_to(src_dir)
        rel_str = str(rel).replace("\\", "/")
        # 如果就在当前目录
        if str(Path(vcxproj_dir).relative_to(src_dir)) == str(rel.parent):
            return rel.name
        return "${SRC_DIR}/" + rel_str
    except ValueError:
        return str(abs_path).replace("\\", "/")


def normalize_include(raw_path, vcxproj_dir, src_dir):
    """转换包含目录路径，处理 MSBuild 宏。"""
    p = raw_path.strip().replace("\\", "/")

    # 过滤占位符
    if p in ("%(AdditionalIncludeDirectories)", ""):
        return None

    # 替换常见 MSBuild 宏
    macro_map = {
        "$(SolutionDir)": "${SRC_DIR}/../",
        "$(ProjectDir)": "${CMAKE_CURRENT_SOURCE_DIR}/",
        "$(IntDir)": "${CMAKE_CURRENT_BINARY_DIR}/",
        "$(SRCDIR)": "${SRC_DIR}",
    }
    for macro, replacement in macro_map.items():
        if p.startswith(macro):
            p = replacement + p[len(macro):]
            return p

    if p.startswith("$"):
        return p  # 其他 CMake 变量，直接保留

    # 解析为绝对路径
    try:
        if os.path.isabs(p):
            abs_path = Path(p)
        else:
            abs_path = (Path(vcxproj_dir) / p).resolve()
        rel = abs_path.relative_to(src_dir)
        return "${SRC_DIR}/" + str(rel).replace("\\", "/")
    except ValueError:
        return str(abs_path).replace("\\", "/") if not os.path.isabs(p) else p


# ---------------------------------------------------------------------------
# vcxproj 解析
# ---------------------------------------------------------------------------
class VcxprojInfo:
    def __init__(self):
        self.project_name = ""
        self.target_name = ""
        self.config_type = "StaticLibrary"  # StaticLibrary / DynamicLibrary / Application
        self.sources = []           # list of (cmake_path, no_pch, enable_eh)
        self.includes = []          # project-specific includes (filtered)
        self.defines = []           # project-specific defines (filtered)
        self.release_defines = []   # Release 特有 defines
        self.link_deps = []         # AdditionalDependencies (all configs)
        self.pch_header = ""        # e.g. "cbase.h"
        self.pch_source = ""        # e.g. "stdafx.cpp" — 创建 PCH 的文件
        self.output_name = ""       # 最终输出文件名（不含扩展名）
        self.additional_opts = []   # AdditionalOptions
        self.no_manifest = False    # GenerateManifest=false
        self.ignore_import_lib = False  # IgnoreImportLibrary=true
        self.vcxproj_path = ""
        self.vcxproj_dir = ""


def parse_vcxproj(vcxproj_path, src_dir):
    info = VcxprojInfo()
    info.vcxproj_path = str(vcxproj_path)
    info.vcxproj_dir = str(Path(vcxproj_path).parent)

    try:
        tree = ET.parse(vcxproj_path)
    except ET.ParseError as e:
        print(f"  警告：解析 {vcxproj_path} 失败: {e}", file=sys.stderr)
        return info

    root = tree.getroot()

    # --- ProjectName / GUID ---
    for pg in root.findall(tag("PropertyGroup")):
        if pg.get("Label") == "Globals":
            name = pg.find(tag("ProjectName"))
            info.project_name = name.text.strip() if name is not None and name.text else ""
            tn = pg.find(tag("TargetName"))
            if tn is not None and tn.text:
                info.target_name = tn.text.strip()

    if not info.project_name:
        info.project_name = Path(vcxproj_path).stem
    if not info.target_name:
        info.target_name = info.project_name

    # 如果 target_name 包含空格或特殊字符，回退到 vcxproj stem
    if re.search(r'[ ()\[\]]', info.target_name):
        info.target_name = Path(vcxproj_path).stem

    # --- ConfigurationType（取第一个 Configuration PropertyGroup）---
    for pg in root.findall(tag("PropertyGroup")):
        if pg.get("Label") == "Configuration":
            ct = pg.find(tag("ConfigurationType"))
            if ct is not None and ct.text:
                info.config_type = ct.text.strip()
                break

    # --- TargetName / OutputName（从非 Globals PropertyGroup 取）---
    for pg in root.findall(tag("PropertyGroup")):
        label = pg.get("Label", "")
        if label in ("Globals", "Configuration", "UserMacros"):
            continue
        tn = pg.find(tag("TargetName"))
        if tn is not None and tn.text and not info.output_name:
            info.output_name = tn.text.strip()

    if not info.output_name:
        info.output_name = info.target_name

    # --- ItemDefinitionGroup（编译/链接设置）---
    debug_defines = []
    release_defines = []
    debug_includes = []
    release_includes = []
    all_link_deps = set()

    for idg in root.findall(tag("ItemDefinitionGroup")):
        cfg = get_condition_config(idg)

        # 编译器设置
        cl = idg.find(tag("ClCompile"))
        if cl is not None:
            # PreprocessorDefinitions
            pd = cl.find(tag("PreprocessorDefinitions"))
            if pd is not None and pd.text:
                defs = [d.strip() for d in pd.text.replace(";", "\n").split("\n")
                        if d.strip() and d.strip() not in GLOBAL_DEFINES
                        and not d.strip().startswith("%")
                        and not any(d.strip().startswith(p) for p in GLOBAL_DEFINE_PREFIXES_TO_SKIP)]
                if cfg == "Debug":
                    debug_defines = defs
                elif cfg == "Release":
                    release_defines = defs

            # AdditionalIncludeDirectories
            aid = cl.find(tag("AdditionalIncludeDirectories"))
            if aid is not None and aid.text:
                raw_incs = [p.strip() for p in re.split(r"[;,]", aid.text)
                            if p.strip() and p.strip() not in GLOBAL_INCLUDES]
                conv_incs = []
                for ri in raw_incs:
                    ci = normalize_include(ri, info.vcxproj_dir, src_dir)
                    if ci and ci not in GLOBAL_INCLUDES:
                        conv_incs.append(ci)
                if cfg == "Debug":
                    debug_includes = conv_incs
                elif cfg == "Release":
                    release_includes = conv_incs

            # PCH 设置
            pch = cl.find(tag("PrecompiledHeader"))
            if pch is not None and pch.text == "Use":
                pch_file = cl.find(tag("PrecompiledHeaderFile"))
                if pch_file is not None and pch_file.text:
                    info.pch_header = pch_file.text.strip()

            # AdditionalOptions
            ao = cl.find(tag("AdditionalOptions"))
            if ao is not None and ao.text and cfg == "Debug":
                opts = ao.text.replace("%(AdditionalOptions)", "").strip()
                if opts:
                    info.additional_opts.append(opts)

        # 链接器设置
        link = idg.find(tag("Link"))
        if link is not None:
            ad = link.find(tag("AdditionalDependencies"))
            if ad is not None and ad.text:
                for dep in re.split(r"[;,]", ad.text):
                    dep = dep.strip()
                    if dep and not dep.startswith("%"):
                        all_link_deps.add(dep)

            gm = link.find(tag("GenerateManifest"))
            if gm is not None and gm.text == "false":
                info.no_manifest = True

            iil = link.find(tag("IgnoreImportLibrary"))
            if iil is not None and iil.text == "true":
                info.ignore_import_lib = True

        # Lib 组（静态库链接到 DLL 时）
        lib_elem = idg.find(tag("Lib"))
        if lib_elem is not None:
            ad = lib_elem.find(tag("AdditionalDependencies"))
            if ad is not None and ad.text:
                for dep in re.split(r"[;,]", ad.text):
                    dep = dep.strip()
                    if dep and not dep.startswith("%"):
                        all_link_deps.add(dep)

    # 合并 Debug+Release 的包含目录（取 Debug 为主）
    info.includes = debug_includes or release_includes

    # 合并 defines（Debug 为主，过滤掉和 Release 完全相同的）
    info.defines = debug_defines or release_defines
    # Release 特有 defines（在 Release 有、Debug 没有的）
    debug_set = set(debug_defines)
    info.release_defines = [d for d in release_defines if d not in debug_set]

    info.link_deps = sorted(all_link_deps)

    # --- 源文件（ClCompile + ClInclude）---
    pch_creators = []

    for item_group in root.findall(tag("ItemGroup")):
        for cc in item_group.findall(tag("ClCompile")):
            path = cc.get("Include", "").strip()
            if not path:
                continue

            cmake_path = normalize_path(path, info.vcxproj_dir, src_dir)

            # Per-file PCH 设置
            no_pch = False
            enable_eh = False

            # 检查所有子元素（可能有多个 Condition）
            for pch_elem in cc.findall(tag("PrecompiledHeader")):
                if pch_elem.text == "NotUsing":
                    no_pch = True
                elif pch_elem.text == "Create":
                    info.pch_source = cmake_path
                    pch_creators.append(cmake_path)

            for eh_elem in cc.findall(tag("ExceptionHandling")):
                if eh_elem.text in ("Sync", "SyncCThrow", "Async"):
                    enable_eh = True

            info.sources.append((cmake_path, no_pch, enable_eh))

    # 如果找到了 PCH 创建文件，标记该文件不需要 SKIP（它就是 PCH 创建者）
    if info.pch_source:
        new_sources = []
        for (p, no_pch, eh) in info.sources:
            if p == info.pch_source:
                new_sources.append((p, True, eh))  # PCH creator 也标记 no_pch（CMake 会自动处理）
            else:
                new_sources.append((p, no_pch, eh))
        info.sources = new_sources

    return info


# ---------------------------------------------------------------------------
# CMakeLists.txt 生成
# ---------------------------------------------------------------------------
def classify_dep(dep, known_targets, src_dir):
    """
    将一个链接依赖分类为：
    - ("target", name)     → CMake 已知目标
    - ("prebuilt", name)   → prebuilt:: IMPORTED 目标
    - ("system", name)     → 系统库，直接写名称
    - ("path", path)       → 绝对/相对路径的 .lib
    - ("skip", name)       → 忽略（常见宏或无效项）
    """
    lower = dep.lower()

    # 忽略项
    if dep.startswith("%(") or dep.strip() == "":
        return ("skip", dep)

    # 含路径的 .lib（如 ..\lib\public\tier0.lib 或 ..\dx9sdk\lib\dinput8.lib）
    if "\\" in dep or "/" in dep:
        # 提取文件名查是否为 CMake 目标
        base = Path(dep.replace("\\", "/")).stem.lower()
        if base in {t.lower() for t in known_targets}:
            # 找到匹配的目标名
            for t in known_targets:
                if t.lower() == base:
                    return ("target", t)
        return ("path", dep)

    # 纯文件名
    base = Path(dep).stem.lower()
    ext = Path(dep).suffix.lower()

    if ext not in (".lib", ""):
        return ("skip", dep)

    # CMake 目标（忽略大小写比较）
    for t in known_targets:
        if t.lower() == base:
            return ("target", t)

    # 系统库
    if dep.lower() in {s.lower() for s in SYSTEM_LIBS}:
        return ("system", dep)

    # prebuilt:: 目标（lib/public 或 lib/common 中已知的）
    prebuilt_names = {
        "steam_api", "libprotobuf", "libz", "nvtristrip", "nvtc",
        "vmpi", "socketlib", "steamdatagramlib", "gcsdk", "gcsdk_gc",
        "libcef", "dmeutils", "fbxutils", "togl", "ati_compress_mt_vc8",
        "steam", "bzip2", "jpeglib", "libjpeg", "libpng", "mss32",
        "mxtoolkitwin32", "havana_constraints", "hk_base", "hk_math",
        "ivp_compactbuilder", "ivp_physics", "cryptlib",
    }
    if base in prebuilt_names:
        return ("prebuilt", base)

    # 其他：直接写出，加注释
    return ("system", dep)


def generate_cmakelists(info, src_dir, dry_run=False):
    """为一个 VcxprojInfo 生成 CMakeLists.txt 内容。"""
    lines = []
    a = lines.append

    target = info.target_name
    cfg_type = info.config_type
    # CMake 变量名：只保留字母、数字、下划线
    var_name = re.sub(r'[^A-Za-z0-9_]', '_', target.upper())

    a(f"# {'=' * 77}")
    a(f"# {info.project_name}")
    a(f"# Generated from: {Path(info.vcxproj_path).name}")
    a(f"# {'=' * 77}")
    a("")

    # --- 源文件列表 ---
    a(f"set({var_name}_SOURCES")
    for (path, no_pch, eh) in info.sources:
        comment = ""
        if no_pch and info.pch_header:
            comment += "  # NO_PCH"
        if eh:
            comment += "  # EH"
        a(f"    {path}{comment}")
    a(")")
    a("")

    # --- add_library / add_executable ---
    if cfg_type == "StaticLibrary":
        a(f"add_library({target} STATIC ${{{var_name}_SOURCES}})")
    elif cfg_type == "DynamicLibrary":
        a(f"add_library({target} SHARED ${{{var_name}_SOURCES}})")
    else:  # Application
        a(f"add_executable({target} ${{{var_name}_SOURCES}})")
    a("")

    # --- 基础设置 ---
    a(f"valve_apply_base_settings({target})")
    a("")

    # --- 项目特有包含目录 ---
    if info.includes:
        # 去重
        seen = set()
        unique_incs = []
        for inc in info.includes:
            if inc not in seen:
                seen.add(inc)
                unique_incs.append(inc)
        a(f"target_include_directories({target} PRIVATE")
        for inc in unique_incs:
            a(f"    {inc}")
        a(")")
        a("")

    # --- 项目特有预处理器定义 ---
    # 去重（保持顺序）
    seen_defs = set()
    all_defs = []
    for d in info.defines:
        if d not in seen_defs:
            seen_defs.add(d)
            all_defs.append(d)

    if all_defs:
        a(f"target_compile_definitions({target} PRIVATE")
        release_set = set(info.release_defines)
        debug_set = set(all_defs)
        for d in all_defs:
            if d in release_set and d not in debug_set - release_set:
                # 两种配置都有 → 普通 define
                a(f"    {d}")
            else:
                a(f"    {d}")
        # Release 特有（不在 debug defines 中的）
        for d in info.release_defines:
            if d not in debug_set:
                a(f"    $<$<CONFIG:Release>:{d}>")
        a(")")
        a("")

    # --- PCH ---
    if info.pch_header:
        a(f"target_precompile_headers({target} PRIVATE")
        a(f'    "$<$<COMPILE_LANGUAGE:CXX>:{info.pch_header}>"')
        a(")")
        a("")

    # --- per-file 设置 ---
    no_pch_files = [p for (p, no_pch, _) in info.sources if no_pch and info.pch_header]
    eh_files = [p for (p, _, eh) in info.sources if eh]

    if no_pch_files:
        a("# 这些文件不使用预编译头")
        a("set_source_files_properties(")
        for f in no_pch_files:
            a(f"    {f}")
        a("    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)")
        a("")

    if eh_files:
        a("# 这些文件需要单独开启异常处理")
        for f in eh_files:
            a(f'set_source_files_properties({f} PROPERTIES COMPILE_OPTIONS "/EHsc")')
        a("")

    # --- 链接依赖 ---
    known_targets = KNOWN_CMAKE_TARGETS
    targets_deps = []
    prebuilt_deps = []
    system_deps = []
    other_deps = []

    for dep in info.link_deps:
        kind, val = classify_dep(dep, known_targets, src_dir)
        if kind == "target":
            targets_deps.append(val)
        elif kind == "prebuilt":
            prebuilt_deps.append(f"prebuilt::{val}")
        elif kind == "system":
            system_deps.append(val)
        elif kind == "path":
            other_deps.append(dep.replace("\\", "/"))
        # skip → 忽略

    all_link = targets_deps + prebuilt_deps + system_deps + other_deps
    if all_link:
        a(f"target_link_libraries({target} PRIVATE")
        if targets_deps:
            a("    # CMake 构建目标")
            for d in targets_deps:
                a(f"    {d}")
        if prebuilt_deps:
            a("    # 预构建库")
            for d in prebuilt_deps:
                a(f"    {d}")
        if system_deps:
            a("    # 系统库")
            for d in system_deps:
                a(f"    {d}")
        if other_deps:
            a("    # 其他（路径引用）")
            for d in other_deps:
                a(f"    \"{d}\"")
        a(")")
        a("")

    # --- Protobuf 生成文件（.pb.cc / .pb.h）---
    pb_sources = [p for (p, _, _) in info.sources if p.endswith(".pb.cc") or p.endswith(".pb.cc  # NO_PCH")]
    # 清理注释部分获取真实路径
    pb_sources_clean = []
    for s in info.sources:
        p = s[0]
        if ".pb.cc" in p or ".pb.h" in p:
            pb_sources_clean.append(p.split("#")[0].strip())

    if pb_sources_clean:
        a("# protobuf 生成文件：标记为 GENERATED（由 protoc 生成，不检查是否存在）")
        a("set_source_files_properties(")
        for p in pb_sources_clean:
            a(f"    {p}")
        a("    PROPERTIES GENERATED TRUE)")
        a("")
        a("# 如需重新生成 .proto 文件，可运行：")
        a("# cmake --build . --target generate_proto  （需先在 CMakeLists.txt 中添加自定义目标）")
        a("")

    # --- 输出设置 ---
    if info.output_name and info.output_name != target:
        a(f"set_target_properties({target} PROPERTIES OUTPUT_NAME \"{info.output_name}\")")
        a("")

    if cfg_type == "StaticLibrary":
        a(f"valve_set_static_lib_output({target})")
    elif cfg_type == "DynamicLibrary":
        a(f"valve_set_dll_importlib_output({target})")
        a("")
        # 判断复制目标
        # game/client → GAME_CSGO_BIN, game/server → GAME_CSGO_BIN
        vcxproj_rel = Path(info.vcxproj_path).relative_to(src_dir)
        parts = vcxproj_rel.parts
        if len(parts) >= 2 and parts[0] == "game":
            dest = "${GAME_CSGO_BIN}"
        else:
            dest = "${GAME_BIN_DIR}"
        a(f"valve_publish_dll({target} \"{dest}\")")
    else:  # Application
        a(f"valve_publish_exe({target} \"${{GAME_DIR}}\")")

    a("")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# 目录树处理
# ---------------------------------------------------------------------------
def build_dir_tree(vcxproj_list, src_dir):
    """
    返回 { rel_dir: [vcxproj_path, ...] } 按目录分组的映射。
    rel_dir 是相对于 src_dir 的路径。
    """
    tree = defaultdict(list)
    for vp in vcxproj_list:
        rel = Path(vp).parent.relative_to(src_dir)
        tree[rel].append(vp)
    return tree


def get_subdirs_with_cmake(rel_dir, all_dirs):
    """返回 rel_dir 的直接子目录（那些也有 CMakeLists.txt 的目录）。"""
    subdirs = set()
    for d in all_dirs:
        try:
            rel_to_parent = d.relative_to(rel_dir)
            parts = rel_to_parent.parts
            if len(parts) == 1:  # 直接子目录
                subdirs.add(parts[0])
        except ValueError:
            pass
    return sorted(subdirs)


def generate_dir_cmakelists(rel_dir, vcxprojs_in_dir, all_dirs, src_dir, infos):
    """生成一个目录的 CMakeLists.txt 内容（可能包含多个目标 + subdirectory 调用）。"""
    lines = []

    subdirs = get_subdirs_with_cmake(rel_dir, all_dirs)

    # 添加本目录中的所有目标
    for vp in vcxprojs_in_dir:
        info = infos.get(str(vp))
        if info:
            lines.append(generate_cmakelists(info, src_dir))

    # 添加子目录
    if subdirs:
        lines.append("")
        for sd in subdirs:
            lines.append(f"add_subdirectory({sd})")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# 生成 src/CMakeLists.txt
# ---------------------------------------------------------------------------
def generate_src_cmakelists(all_top_dirs):
    """生成 src/CMakeLists.txt 的内容。"""
    lines = [
        "# =============================================================================",
        "# src/CMakeLists.txt",
        "# 自动生成 — 所有 Source Engine 编译目标",
        "# =============================================================================",
        "",
        "list(APPEND CMAKE_MODULE_PATH \"${CMAKE_CURRENT_SOURCE_DIR}/cmake\")",
        "include(ValveBase)",
        "include(ValvePostBuild)",
        "include(ValvePrebuiltLib)",
        "",
        "# 注册预构建的第三方库",
        "valve_setup_prebuilt_libs()",
        "valve_setup_system_libs()",
        "",
        "# -----------------------------------------------------------------------------",
        "# 目标目录（按依赖顺序排列：底层库优先）",
        "# -----------------------------------------------------------------------------",
    ]

    # 优先顺序
    PRIORITY = [
        "tier0", "tier1", "tier2", "tier3",
        "interfaces", "appframework", "vstdlib", "unitlib",
        "bitmap", "mathlib", "filesystem",
        "bonesetup", "choreoobjects", "datamodel",
    ]

    priority_dirs = []
    other_dirs = []
    for d in sorted(all_top_dirs):
        d_str = str(d)
        if any(d_str == p or d_str.startswith(p + "/") or d_str.startswith(p + "\\")
               for p in PRIORITY):
            priority_dirs.append(d_str)
        else:
            other_dirs.append(d_str)

    for d in priority_dirs + other_dirs:
        lines.append(f"add_subdirectory({d})")

    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# 主函数
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="从 vcxproj 生成 CMakeLists.txt")
    parser.add_argument("--dry-run", action="store_true", help="仅预览，不写文件")
    parser.add_argument("--target", help="仅处理指定目标（vcxproj 相对路径或目标名）")
    parser.add_argument("--src-dir", default=None, help="src/ 目录路径（默认：脚本所在目录的父目录）")
    args = parser.parse_args()

    # 确定 src_dir
    script_dir = Path(__file__).parent
    src_dir = Path(args.src_dir) if args.src_dir else script_dir.parent
    src_dir = src_dir.resolve()

    if not src_dir.is_dir():
        print(f"错误：src 目录不存在: {src_dir}", file=sys.stderr)
        sys.exit(1)

    print(f"SRC_DIR: {src_dir}")

    # 发现所有 vcxproj
    all_vcxprojs = sorted(src_dir.rglob("*.vcxproj"))
    print(f"发现 {len(all_vcxprojs)} 个 .vcxproj 文件")

    # 过滤 --target
    if args.target:
        all_vcxprojs = [v for v in all_vcxprojs
                        if args.target in str(v)]
        print(f"过滤后: {len(all_vcxprojs)} 个")

    # 解析所有 vcxproj
    infos = {}
    for vp in all_vcxprojs:
        print(f"  解析: {vp.relative_to(src_dir)}")
        info = parse_vcxproj(vp, src_dir)
        infos[str(vp)] = info

    # 按目录分组
    dir_tree = build_dir_tree(all_vcxprojs, src_dir)
    all_dirs = set(dir_tree.keys())

    # 生成所有 CMakeLists.txt
    files_written = 0

    # 为每个有 vcxproj 的目录生成 CMakeLists.txt
    for rel_dir, vcxprojs_in_dir in sorted(dir_tree.items()):
        cmake_path = src_dir / rel_dir / "CMakeLists.txt"
        content = generate_dir_cmakelists(
            rel_dir, vcxprojs_in_dir, all_dirs, src_dir, infos
        )
        if args.dry_run:
            print(f"\n{'='*60}")
            print(f"# {cmake_path.relative_to(src_dir.parent)}")
            print(content[:500] + "..." if len(content) > 500 else content)
        else:
            cmake_path.write_text(content, encoding="utf-8")
            files_written += 1
            print(f"  写入: {cmake_path.relative_to(src_dir)}")

    # 为中间目录（无 vcxproj 但有子目录含 vcxproj）生成 pass-through CMakeLists.txt
    intermediate_dirs = set()
    for rel_dir in all_dirs:
        parts = rel_dir.parts
        for i in range(1, len(parts)):
            parent = Path(*parts[:i])
            if parent not in all_dirs:
                intermediate_dirs.add(parent)

    for rel_dir in sorted(intermediate_dirs):
        cmake_path = src_dir / rel_dir / "CMakeLists.txt"
        if cmake_path.exists():
            continue  # 已经有内容，跳过
        subdirs = get_subdirs_with_cmake(rel_dir, all_dirs | intermediate_dirs)
        lines = [f"# {rel_dir} — pass-through", ""]
        for sd in subdirs:
            lines.append(f"add_subdirectory({sd})")
        content = "\n".join(lines) + "\n"
        if args.dry_run:
            print(f"\n# INTERMEDIATE: {cmake_path.relative_to(src_dir.parent)}")
            print(content)
        else:
            cmake_path.write_text(content, encoding="utf-8")
            files_written += 1
            print(f"  写入(中间): {cmake_path.relative_to(src_dir)}")

    # 生成 src/CMakeLists.txt
    top_dirs = set()
    for rel_dir in all_dirs:
        top_dirs.add(rel_dir.parts[0])
    for rel_dir in intermediate_dirs:
        top_dirs.add(rel_dir.parts[0])

    src_cmake = src_dir / "CMakeLists.txt"
    src_content = generate_src_cmakelists(top_dirs)
    if args.dry_run:
        print(f"\n# src/CMakeLists.txt")
        print(src_content[:1000])
    else:
        src_cmake.write_text(src_content, encoding="utf-8")
        files_written += 1
        print(f"\n写入: src/CMakeLists.txt")

    print(f"\n完成！共写入 {files_written} 个文件。")


if __name__ == "__main__":
    main()
