# Maya DMX Plugin Scaffold

This directory hosts the Maya 2022.5 DMX importer/exporter plugin scaffold.

## Build

Standalone x64 configure:

```powershell
cmake -S dcc_plugin -B build\maya_dmx -A x64
cmake --build build\maya_dmx --config Release
```

Batch wrappers:

```bat
dcc_plugin\CreatePluginSolution.bat
dcc_plugin\BuildPlugin.bat
```

Standalone sample conversion utility:

```powershell
dcc_plugin\bin\Release\maya_dmx_sample_tool.exe input.dmx output.dmxb
dcc_plugin\bin\Release\maya_dmx_sample_tool.exe input.dmxb output.dmx
```

Sample regression:

```bat
dcc_plugin\RunSampleRegression.bat
```

Maya standalone regression:

```bat
dcc_plugin\RunMayaBatchRegression.bat
```

Workflow command inside Maya:

```mel
mayaDmxWorkflow -savePreset "default" -outputDirectory "D:/exports" -encoding "binary" -exportSkin true -exportDeltaStates true;
mayaDmxWorkflow -listPresets;
mayaDmxWorkflow -loadPreset "default";
mayaDmxWorkflow -saveBatch "characters" -batchEntry "|root_chr_a|characters/chr_a.dmx" -batchEntry "|root_chr_b|characters/chr_b.dmx";
mayaDmxWorkflow -listBatches;
mayaDmxWorkflow -loadBatch "characters";
```

Or through CMake:

```powershell
cmake --build build\maya_dmx --config Release --target maya_dmx_sample_regression
```

Repository configure with plugin enabled:

```powershell
cmake -S . -B build\repo_x64 -A x64 -DBUILD_MAYA_DMX_PLUGIN=ON
```

The plugin prefers x64 builds because Maya 2022.5 on Windows is x64.

## Maya Module Install

Install the built plugin as a Maya module:

```bat
dcc_plugin\InstallPluginModuleToMaya.bat
```

This writes `%USERPROFILE%\Documents\maya\modules\maya_dmx.mod` and copies the plugin to `dcc_plugin\maya_module\plug-ins\windows\2022\`.

`RunMayaBatchRegression.bat` defaults to `C:\Program Files\Autodesk\Maya2022\bin\mayapy.exe`. If Maya is installed elsewhere, set `MAYA_PYTHON_EXE_OVERRIDE` before running it.

`mayaDmxWorkflow` is the current workflow-layer skeleton. It persists export presets and batch manifests through Maya optionVars, but it does not execute batch exports yet.

## Current Import Scope

The importer now supports a minimal text DMX subset:

- top-level `DmeModel`, `DmeDag`, `DmeJoint`
- inline or referenced `DmeTransform`
- inline `DmeMesh` with `bindState/baseStates`, `positions`, `positionsIndices`, `normals`, `normalsIndices`, `textureCoordinates`, `textureCoordinatesIndices`, `jointCount`, `jointWeights`, `jointIndices`, `faceSets`, `faces`
- `name`, `position`, `orientation`, `children`, `jointList`, `upAxis`

Current limitations:

- supports text DMX and a minimal binary DMX subset for the same attribute set
- hierarchy / transform / basic polygon mesh import
- supports primary UV set and face-vertex normals for text DMX
- supports basic skin weights via `jointList + jointCount + jointWeights + jointIndices`
- restores basic face-set material slots as Maya shading groups
- supports minimal position-only `deltaStates` import by creating Maya blendShape targets
- binary DMX import currently covers only the attribute types emitted by this plugin
- no full material networks, advanced combination operators, tangents, extra UV channels, or full Valve binary DMX coverage yet

## Current Export Scope

The exporter now writes a minimal text DMX scene:

- top-level `DmeModel`
- recursive `DmeDag` / `DmeJoint` hierarchy
- local `DmeTransform` translation + quaternion rotation
- inline `DmeMesh` with `bindState`, `positions`, `positionsIndices`, `normals`, `normalsIndices`, `textureCoordinates`, `textureCoordinatesIndices`, `jointCount`, `jointIndices`, `jointWeights`, `faceSets`, `faces`
- exports active selection roots when using Maya "Export Selection", otherwise exports top-level DAG roots
- writes `jointList` from the full exported joint hierarchy so skin indices line up with exported influences
- preserves basic face-set names from Maya shading groups when exporting polygon assignments
- exports minimal position-only blendShape targets as `DmeVertexDeltaData` entries under `deltaStates`
- can emit binary DMX when exporting to `.dmxb` / `.dmxbin`, or when the translator options include `binary=1` or `encoding=binary`

Current limitations:

- text DMX remains the default export mode
- binary DMX export currently writes only the minimal attribute subset used by this plugin
- only basic material slot / shading group names are preserved; full Hypershade networks are not exported
- delta export currently covers only position deltas from basic blendShape targets
- no advanced combination operators, tangents, extra UV sets, or full Valve binary DMX coverage yet
- skin export currently writes basic vertex weights only and does not export bind pose extras or advanced deformer metadata

Sample file for manual Maya import:

- `dcc_plugin/samples/simple_hierarchy.dmx`
- `dcc_plugin/samples/simple_hierarchy.dmxb`
- `dcc_plugin/samples/simple_blendshape.dmx`
- `dcc_plugin/samples/simple_blendshape.dmxb`
- `dcc_plugin/samples/simple_mesh.dmx`
- `dcc_plugin/samples/simple_mesh.dmxb`
- `dcc_plugin/samples/simple_skinned_mesh.dmx`
- `dcc_plugin/samples/simple_skinned_mesh.dmxb`
- `dcc_plugin/samples/complex_chr_mesh.dmx`
- `dcc_plugin/samples/complex_chr_mesh.dmxb`

## Layout

- `cmake/` - Maya SDK detection and plugin target helpers
- `maya_module/` - local module payload staged for Maya installation
- `samples/` - small text/binary DMX files for manual plugin testing
- `tools/` - small command-line helpers for sample conversion and regression prep, including Maya standalone regression
- `src/common/` - shared utilities and diagnostics
- `src/importer/` - DMX importer translator
- `src/exporter/` - DMX exporter translator
- `src/workflow/` - workflow-layer preset and batch manifest management for future batch export features
- `src/plugin/` - plugin entry point and translator registration
