# AGENTS.md

This file provides unified guidance to coding agents working in this repository, including Claude Code and Codex CLI.

## Project Overview

**csgo-src** is a refactored version of the leaked CS:GO (Counter-Strike: Global Offensive) Source Engine source code. Goals: improved stability and playability. Key changes from the original leak:
- Scaleform removed
- Filesystem replaced with the TF2 leak version (less hardcoded, supports `custom/` folder)
- VGUI and GameUI ported from the TF2 leak
- Some weapon recoil and econ code imported from Kisak-Strike

**Known issues**:
- `sv_pure` is likely broken
- The game crashes on shutdown, likely related to `g_pCVar` destruction order

## Build Requirements

- **Visual Studio 2022** (MSVC only; GCC/Clang not supported)
- **CMake 3.20+**
- **Windows SDK** (default: `10.0.26200`, configurable via `-DWINDOWS_SDK_VERSION=`)
- **Target**: Win32 (x86) only; 64-bit builds are not supported

## Build Commands

**Generate Visual Studio solution:**
```bat
CmakeCreateOptionalSolution.bat
```
Outputs `build/CSGO.sln`.

**Build from command line:**
```bat
CmakeBuildSolution.bat
CmakeBuildSolution.bat Debug
```

**Manual CMake:**
```bat
cmake -B build -A Win32 -S .
cmake --build build --config Release --parallel
```

**Open in Visual Studio:**
```bat
start build\CSGO.sln
```

## Output Locations

- `game/bin/` - common engine binaries (`engine.dll`, `launcher.dll`, etc.)
- `game/csgo/bin/` - game-specific binaries (`client.dll`, `server.dll`)
- `game/` - executables (`csgo.exe`, dedicated server)

## Architecture

### Dependency Tiers

```text
tier0 → tier1 → tier2 → tier3
              ↓
         vstdlib, mathlib, interfaces
              ↓
    appframework, filesystem, bitmap, ...
              ↓
         engine / game DLLs
```

- **tier0**: memory allocators, CPU detection, logging, `platform.h`
- **tier1**: containers (`CUtlVector`, `CUtlMap`), strings, key-values
- **tier2**: material system interfaces, render contexts
- **tier3**: higher-level abstractions over tier2

### Core Modules

| Module | Role |
|---|---|
| `src/engine/` | Core Source Engine (main game loop, networking, console) |
| `src/engine_ds/` | Dedicated server engine variant |
| `src/launcher/` + `src/launcher_main/` | Entry point; bootstraps engine DLL |
| `src/game/client/` | Client-side DLL (rendering hooks, UI, prediction) |
| `src/game/server/` | Server-side DLL (game logic, entities, physics) |
| `src/game/shared/` | Code compiled into both client and server |
| `src/materialsystem/` | Shader/material pipeline |
| `src/studiorender/` | MDL model rendering |
| `src/vgui2/` + `src/vguimatsurface/` | UI framework |
| `src/gameui/` | In-game menus and HUD |
| `src/filesystem/` | Virtual filesystem abstraction |
| `src/soundsystem/` | Audio engine |
| `src/particles/` | Particle effects |

## CMake Conventions

All modules use helpers from `src/cmake/`:

- `valve_apply_base_settings(TARGET)`: applies global compiler flags, defines, and include paths. Use on every target.
- `valve_force_include_platform_h(TARGET)`: force-includes `tier0/platform.h`. Use on all targets except tier0 itself.
- `valve_target_sources_no_pch(TARGET [FILES...])`: marks files to skip precompiled headers.
- `valve_source_enable_exceptions(FILES...)`: enables `/EHsc` per-file because exceptions are off globally.

Global include roots are configured in `ValveBase.cmake`:
- `src/common/`
- `src/public/`
- `src/public/tier0/`
- `src/public/tier1/`

Prebuilt third-party libraries (protobuf, zlib, libpng, SDL2, DirectX) are registered via `valve_setup_prebuilt_libs()` in `src/cmake/ValvePrebuiltLib.cmake`.

## Adding a New Module

1. Create `src/<module>/CMakeLists.txt`
2. Define the target with `add_library` or `add_executable`
3. Call `valve_apply_base_settings(<target>)` and `valve_force_include_platform_h(<target>)`
4. Add `add_subdirectory(<module>)` to `src/CMakeLists.txt` in dependency order

## Runtime Setup

The game needs an existing CS:GO or Source SDK installation pointed at `game/`. Binaries built here go into `game/bin/` and `game/csgo/bin/` alongside assets from the retail game.

## Agent Working Notes

- Prefer `rg` and `rg --files` for code search
- Preserve existing project structure and CMake helper usage
- Do not introduce 64-bit, GCC, or Clang assumptions into build changes
- When modifying modules, verify dependency direction before changing `src/CMakeLists.txt`
- Treat the shutdown crash and `sv_pure` behavior as known issues unless the task is explicitly about fixing them
- Use [Plan.md](D:/_Code_Here/Git/csgo-src/Plan.md) as the repository-wide repair log and rolling project plan. It records confirmed issues that still need changes or fixes.
- After every task, summarize newly discovered issues, resolved items, or priority changes and update `Plan.md` so the cross-task plan stays current.
