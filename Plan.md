# Project Plan

This file tracks confirmed problems, pending fixes, and follow-up work discovered while operating on the repository.

Update policy:
- After each task, review whether it exposed a new confirmed issue, removed an existing issue, or changed priority.
- Keep this file as the current cross-task repair plan for the repository instead of scattering TODOs across chat history.
- Record only actionable items with enough detail to reproduce or continue work quickly.

## Active Build Issues

### 1. `client_csgo` link failures block a full Release build
- Status: Open
- Priority: High
- Target: [src/game/client/CMakeLists.txt](D:/_Code_Here/Git/csgo-src/src/game/client/CMakeLists.txt)
- Evidence:
  - `libprotobuf.lib(common.obj)` reports `_MSC_VER` mismatch (`1600` vs `1900`) when linking `client_csgo`.
  - `base_gcmessages.pb.obj` / `cstrike15_gcmessages.pb.obj` report unresolved `protobuf_AddDesc_steammessages_2eproto`.
  - Many unresolved imports from the client link, including `RandomInt`, `RandomFloat`, `RandomSeed`, `g_pThreadPool`, and `KeyValuesSystem`.
- Likely causes:
  - Bundled protobuf library was built with an older MSVC toolset than the rest of the tree.
  - `client_csgo` is missing one or more dependency libraries that normally provide tier0/tier1/vstdlib/random/threadpool/keyvalues symbols.
  - Generated protobuf sources are not linked together with all required generated translation units.
- Next steps:
  - Validate the new bundled-source `libprotobuf` CMake target and confirm it replaces the old prebuilt library in all protobuf consumers.
  - Compare the client target link set against the original VPC project and missing imported libs.
  - Verify where `steammessages.proto` generated objects should be compiled and linked.

### 2. `togl_togl` fails to compile against current Windows SDK OpenGL headers
- Status: Open
- Priority: High
- Target: [src/togl](D:/_Code_Here/Git/csgo-src/src/togl)
- Evidence:
  - Compilation fails in Windows SDK `um/GL/gl.h` around line 1171 with repeated `C2086`, `C2144`, `C2182`, `C4430`, and `C1003`.
- Likely causes:
  - Conflicting GL macro/type declarations before including `gl.h`.
  - Header include order or platform defines differ from the original Source build assumptions.
- Next steps:
  - Inspect `src/togl` include chain and compare with older Valve build environment.
  - Identify whether `APIENTRY`, `WINGDIAPI`, or `GLAPI` is being redefined before `gl.h`.
  - Consider isolating Windows GL headers behind a compatibility shim if the SDK version changed behavior.

### 3. `tracker_AdminServer` contains API drift from current utility helpers
- Status: Open
- Priority: Medium
- Targets:
  - [RemoteServer.cpp](D:/_Code_Here/Git/csgo-src/src/tracker/AdminServer/RemoteServer.cpp)
  - [serverinfopanel.cpp](D:/_Code_Here/Git/csgo-src/src/tracker/AdminServer/serverinfopanel.cpp)
- Evidence:
  - `RemoteServer.cpp(154)` calls `CUtlBuffer::GetString` with the wrong signature (`C2660`).
  - `serverinfopanel.cpp(249)` references missing identifier `use_V_isspace_instead_of_isspace` (`C3861`).
- Likely causes:
  - Source files were copied from a codebase with different tier1 helper APIs/macros.
- Next steps:
  - Update calls to match the current `CUtlBuffer` interface.
  - Replace raw ctype use with the repo’s approved string/ctype wrappers.

### 4. Several utility targets are missing required platform or external libraries
- Status: Open
- Priority: High
- Targets:
  - [src/utils/binlaunch](D:/_Code_Here/Git/csgo-src/src/utils/binlaunch)
  - [src/utils/vfont](D:/_Code_Here/Git/csgo-src/src/utils/vfont)
  - [src/utils/phonemeextractor](D:/_Code_Here/Git/csgo-src/src/utils/phonemeextractor)
- Evidence:
  - `binlaunch.exe` fails with unresolved `Plat_IsInDebugSession`, `Plat_ExitProcess`, `WriteMiniDump`, `g_pMemAlloc`.
  - `vfont.exe` and `vfont_decompiler.exe` fail with unresolved logging/platform/memory allocation symbols.
  - `phonemeextractor` cannot open `sapi.lib`, and `phonemeextractor_ims.cpp` cannot find `ims_helper/ims_helper.h`.
- Likely causes:
  - Missing linkage to tier0/vstdlib or crash-handling support libs.
  - Hard dependency on SDKs or third-party source trees not present in this checkout.
- Next steps:
  - Compare target link libraries with original project files.
  - Gate optional utilities behind feature checks when dependencies are absent.
  - Document or vendor the missing speech/IMS dependencies if they are still required.

### 5. MFC-dependent tools are only partially gated
- Status: Open
- Priority: Medium
- Targets:
  - [src/utils/hlfaceposer](D:/_Code_Here/Git/csgo-src/src/utils/hlfaceposer)
  - [src/hammer](D:/_Code_Here/Git/csgo-src/src/hammer)
  - [src/utils/FileSystemOpenDialog](D:/_Code_Here/Git/csgo-src/src/utils/FileSystemOpenDialog)
- Evidence:
  - Configure stage already reports `hammer_dll` and `FileSystemOpenDialog` skipped because MFC is not installed.
  - Build still reaches `hlfaceposer.rc` and fails with `RC1015: cannot open include file 'afxres.h'`.
- Likely causes:
  - `hlfaceposer` lacks the same MFC availability guard used by other targets.
- Next steps:
  - Apply consistent CMake feature detection and target skipping for all MFC-bound tools.
  - Document the exact Visual Studio component needed when those tools are desired.

### 6. VScript language targets are incomplete or out of sync
- Status: Open
- Priority: Medium
- Targets:
  - [src/vscript/languages/squirrel/vsquirrel](D:/_Code_Here/Git/csgo-src/src/vscript/languages/squirrel/vsquirrel)
  - [src/vscript/languages/python/vpython](D:/_Code_Here/Git/csgo-src/src/vscript/languages/python/vpython)
  - [src/vscript/languages/gm/vgm](D:/_Code_Here/Git/csgo-src/src/vscript/languages/gm/vgm)
  - [src/vscript/languages/lua/vlua](D:/_Code_Here/Git/csgo-src/src/vscript/languages/lua/vlua)
- Evidence:
  - `vsquirrel.cpp` cannot find `init_nut.h`.
  - `vpython.cpp` cannot find `Python.h`.
  - `vgm.cpp` has syntax errors near lines 744-745.
  - `vlua.cpp` has multiple `IScriptVM::SetValue` overload mismatches and one `SetErrorCallback` signature mismatch.
- Likely causes:
  - Missing generated/bundled headers for optional language runtimes.
  - Source drift between script VM interface and backend implementations.
- Next steps:
  - Decide which scripting backends are optional and gate them accordingly.
  - Fix interface mismatches for the backends intended to remain enabled.
  - Add configure-time checks for Python and any generated squirrel artifacts.

### 7. `vgui_perftest` is missing a required header dependency
- Status: Open
- Priority: Low
- Target: [src/vgui2/vgui_perftest/vgui_perftest.cpp](D:/_Code_Here/Git/csgo-src/src/vgui2/vgui_perftest/vgui_perftest.cpp)
- Evidence:
  - `vgui_perftest.cpp(38)` cannot include `console_logging.h` (`C1083`).
- Likely causes:
  - Missing include directory or stale header path after refactor.
- Next steps:
  - Locate the intended header and restore the include path or update the include directive.

## Environment / Toolchain Notes

### A. Current batch build wrapper needed a compatibility fix
- Status: Done
- Result:
  - [CmakeBuildSolution.bat](D:/_Code_Here/Git/csgo-src/CmakeBuildSolution.bat) now re-runs CMake configure before build and avoids the `--parallel` invocation that caused immediate early exit in this environment.

### B. Bundled protobuf now has a CMake source-build target
- Status: Done
- Result:
  - Added [src/thirdparty/protobuf-2.5.0/CMakeLists.txt](D:/_Code_Here/Git/csgo-src/src/thirdparty/protobuf-2.5.0/CMakeLists.txt) to build `libprotobuf` from bundled 2.5.0 sources.
  - Registered it through [src/thirdparty/CMakeLists.txt](D:/_Code_Here/Git/csgo-src/src/thirdparty/CMakeLists.txt) and moved third-party registration earlier in [src/CMakeLists.txt](D:/_Code_Here/Git/csgo-src/src/CMakeLists.txt) so `prebuilt::libprotobuf` now resolves to the source-built target before consumers are configured.

### C. Optional dependencies detected during configure
- Status: Open
- Notes:
  - Autodesk FBX SDK was unavailable for active x86 toolchain; build falls back to OpenFBX.
  - `utils/studiomdl` reports missing `libedgegeomtool`, so edgegeom support is disabled.
  - MFC was not installed for this machine/toolchain, affecting several tools as noted above.
