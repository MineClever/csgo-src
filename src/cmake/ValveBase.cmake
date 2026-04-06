# =============================================================================
# ValveBase.cmake
# 等价于 vpc_scripts/source_win32_base.vpc
# 提供所有 Valve 目标共享的编译器设置、宏定义和辅助函数。
# =============================================================================

# -----------------------------------------------------------------------------
# 全局预处理器宏（所有目标共享，来自 vcxproj 公共 PreprocessorDefinitions）
# -----------------------------------------------------------------------------
set(VALVE_GLOBAL_DEFINES
    WIN32
    _WIN32
    COMPILER_MSVC
    "_DLL_EXT=.dll"
    CSTRIKE15
    AVI_VIDEO
    WMV_VIDEO
    RAD_TELEMETRY_DISABLED
    "CSTRIKE_REL_BUILD=1"
    _CRT_SECURE_NO_DEPRECATE
    _CRT_NONSTDC_NO_DEPRECATE
    _ALLOW_RUNTIME_LIBRARY_MISMATCH
    _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH
    _ALLOW_MSC_VER_MISMATCH
    VPC
)

if(VALVE_TARGET_ARCH STREQUAL "x64")
    list(APPEND VALVE_GLOBAL_DEFINES
        _WIN64
        COMPILER_MSVC64
    )
else()
    list(APPEND VALVE_GLOBAL_DEFINES COMPILER_MSVC32)
endif()

# Debug 特有
set(VALVE_DEFINES_DEBUG
    _DEBUG
    DEBUG
    "_HAS_ITERATOR_DEBUGGING=0"
)

# Release 特有
set(VALVE_DEFINES_RELEASE
    NDEBUG
)

# -----------------------------------------------------------------------------
# 全局包含目录（所有目标均可见）
# -----------------------------------------------------------------------------
set(VALVE_GLOBAL_INCLUDES
    "${SRC_DIR}/common"
    "${SRC_DIR}/public"
    "${SRC_DIR}/public/tier0"
    "${SRC_DIR}/public/tier1"
)

# -----------------------------------------------------------------------------
# 禁用警告列表（直接从 vcxproj DisableSpecificWarnings 提取）
# 对应 source_win32_base.vpc 中的 $DisableSpecificWarnings 条目
# -----------------------------------------------------------------------------
set(VALVE_DISABLED_WARNINGS
    /wd4061 /wd4062 /wd4091 /wd4097 /wd4100 /wd4121 /wd4127
    /wd4191 /wd4201 /wd4239 /wd4242 /wd4244 /wd4250 /wd4254
    /wd4255 /wd4263 /wd4264 /wd4265 /wd4266 /wd4296 /wd4302
    /wd4311 /wd4316 /wd4324 /wd4350 /wd4351 /wd4355 /wd4365
    /wd4371 /wd4388 /wd4435 /wd4464 /wd4471 /wd4481 /wd4505
    /wd4511 /wd4512 /wd4514 /wd4530 /wd4544 /wd4547 /wd4548
    /wd4571 /wd4574 /wd4577 /wd4587 /wd4611 /wd4619 /wd4623
    /wd4625 /wd4626 /wd4628 /wd4640 /wd4647 /wd4668 /wd4702
    /wd4710 /wd4711 /wd4738 /wd4748 /wd4774 /wd4777 /wd4786
    /wd4820 /wd4826 /wd4868 /wd4883 /wd4917 /wd4928 /wd4946
    /wd4986 /wd4987 /wd4996
    # VS2015+ 特有警告
    /wd5026 /wd5027 /wd5029 /wd5031 /wd5032
)

# -----------------------------------------------------------------------------
# 全局编译选项（等价 source_win32_base.vpc $Compiler 块）
# -----------------------------------------------------------------------------
set(VALVE_COMPILE_OPTIONS
    /MP             # 多处理器编译
    /FS             # 允许并行 cl.exe 安全写入同一 PDB，避免 C1041
    /W3             # 警告级别 3
    /arch:SSE2      # SSE2 指令集
    /fp:fast        # 快速浮点模型
    /Zc:forScope    # for 循环变量作用域遵循标准
    /GR             # 启用 RTTI
    /GF             # 字符串池化（消除重复字符串字面量）
    /nologo
    /GS-            # 关闭缓冲区安全检查（vcxproj: BufferSecurityCheck=false）
    ${VALVE_DISABLED_WARNINGS}
)

# Debug 附加选项
set(VALVE_COMPILE_OPTIONS_DEBUG
    /Od             # 禁用优化
    /ZI             # Edit-and-Continue PDB（调试时支持热重载）
    /Gm-            # 禁用最小重新生成（与 /MP 不兼容）
)

# Release 附加选项
set(VALVE_COMPILE_OPTIONS_RELEASE
    /O2             # 最大速度优化
    /Ob2            # 内联展开：任意合适（AnySuitable）
    /Oi             # 启用固有函数
    /Ot             # 倾向快速代码
    /Gy             # 函数级别链接
    /Zi             # PDB 调试信息（Release 也保留符号）
    /Gm-
)

# -----------------------------------------------------------------------------
# 函数：valve_apply_base_settings(TARGET)
# 将全局编译设置应用到一个目标，等价于 vpc_scripts/source_win32_base.vpc
# -----------------------------------------------------------------------------
function(valve_apply_base_settings TARGET)
    target_include_directories(${TARGET} PRIVATE
        ${VALVE_GLOBAL_INCLUDES}
    )

    target_compile_definitions(${TARGET} PRIVATE
        ${VALVE_GLOBAL_DEFINES}
        $<$<CONFIG:Debug>:${VALVE_DEFINES_DEBUG}>
        $<$<CONFIG:Release>:${VALVE_DEFINES_RELEASE}>
    )

    target_compile_options(${TARGET} PRIVATE
        ${VALVE_COMPILE_OPTIONS}
        $<$<CONFIG:Debug>:${VALVE_COMPILE_OPTIONS_DEBUG}>
        $<$<CONFIG:Release>:${VALVE_COMPILE_OPTIONS_RELEASE}>
    )

    target_link_options(${TARGET} PRIVATE
        $<$<STREQUAL:${VALVE_TARGET_ARCH},x64>:/MACHINE:X64>
        $<$<NOT:$<STREQUAL:${VALVE_TARGET_ARCH},x64>>:/MACHINE:X86>
        /ignore:4221    # 忽略"无公共符号"链接器警告
        $<$<NOT:$<STREQUAL:${VALVE_TARGET_ARCH},x64>>:/SAFESEH:NO>     # vcxproj: ImageHasSafeExceptionHandlers=false
        /NODEFAULTLIB:libc
        /NODEFAULTLIB:libcd
        # memoverride.cpp intentionally redefines CRT allocation functions
        # (_recalloc, _malloc, etc.) to redirect to Valve's custom allocator.
        # With newer Windows SDK (10.0.26100+), libucrt.lib also defines these,
        # causing LNK2005. /FORCE:MULTIPLE allows this and uses memoverride.obj
        # since project objects are linked before default libraries.
        /FORCE:MULTIPLE
        $<$<CONFIG:Release>:/OPT:REF>   # 移除未引用代码
        $<$<CONFIG:Release>:/OPT:ICF>   # 合并相同 COMDAT
    )
endfunction()

# -----------------------------------------------------------------------------
# 函数：valve_force_include_platform_h(TARGET)
# 给目标添加强制包含 tier0/platform.h（tier0 自身不需要调用）
# -----------------------------------------------------------------------------
function(valve_force_include_platform_h TARGET)
    target_compile_options(${TARGET} PRIVATE
        /FI"tier0/platform.h"
    )
endfunction()

# -----------------------------------------------------------------------------
# 函数：valve_target_sources_no_pch(TARGET [FILE...])
# 将指定源文件标记为不使用预编译头（等价 vcxproj per-file PrecompiledHeader=NotUsing）
# -----------------------------------------------------------------------------
function(valve_target_sources_no_pch TARGET)
    set_source_files_properties(${ARGN} PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
    )
endfunction()

# -----------------------------------------------------------------------------
# 函数：valve_source_enable_exceptions(FILE...)
# 给特定源文件单独开启异常处理（vcxproj per-file ExceptionHandling=Sync）
# -----------------------------------------------------------------------------
function(valve_source_enable_exceptions)
    set_source_files_properties(${ARGN} PROPERTIES
        COMPILE_OPTIONS "/EHsc"
    )
endfunction()
