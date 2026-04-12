# Maya DMX Plugin Scaffold

This directory hosts the Maya 2022.5 DMX importer/exporter plugin scaffold.

## Build

Standalone x64 configure:

```powershell
cmake -S dcc_plugin -B dcc_plugin\build -A x64
cmake --build dcc_plugin\build --config Release
```

PDB generation can be controlled explicitly:

```powershell
cmake -S dcc_plugin -B dcc_plugin\build -A x64 -DMAYA_DMX_BUILD_PDB=ON
cmake -S dcc_plugin -B dcc_plugin\build -A x64 -DMAYA_DMX_BUILD_PDB=OFF
```

Batch wrappers:

```bat
dcc_plugin\CreatePluginSolution.bat
dcc_plugin\BuildPlugin.bat
```

`CreatePluginSolution.bat` configures with `MAYA_DMX_BUILD_PDB=ON` so Visual Studio builds keep deployable `.pdb` files. `BuildPlugin.bat` configures with `MAYA_DMX_BUILD_PDB=OFF` and clears stale plugin `.pdb` files before building, so its default Release output only contains the `.mll` binaries.

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

Host environment query:

```bat
dcc_plugin\QueryMayaValidationEnv.bat
```

Interactive Maya validation:

```bat
dcc_plugin\RunMayaInteractiveValidation.bat
```

Workflow command inside Maya:

```mel
mayaDmxWorkflow -savePreset "default" -outputDirectory "D:/exports" -encoding "binary" -exportSkin true -exportDeltaStates true -exportMetadata false;
mayaDmxWorkflow -listPresets;
mayaDmxWorkflow -loadPreset "default";
mayaDmxWorkflow -saveBatch "characters" -batchEntry "|root_chr_a|characters/chr_a.dmx" -batchEntry "|root_chr_b|characters/chr_b.dmx";
mayaDmxWorkflow -listBatches;
mayaDmxWorkflow -listLegacyBatches;
mayaDmxWorkflow -migrateLegacyBatches;
mayaDmxWorkflow -cleanupBatchStorage;
mayaDmxWorkflow -loadBatch "characters";
```

MEL export UI helpers inside Maya:

```mel
source "DmxCreateUI.mel";
MayaDmxShowImportOptions();
MayaDmxShowExportSelectionOptions();
MayaDmxShowExportAllOptions();
```

Or through CMake:

```powershell
cmake --build dcc_plugin\build --config Release --target maya_dmx_sample_regression
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

This writes `%USERPROFILE%\Documents\maya\modules\maya_dmx.mod`, clears the staged module payload directories under `dcc_plugin\maya_module\`, then copies both plugin binaries and any matching `.pdb` files to `dcc_plugin\maya_module\plug-ins\windows\2022\`, and copies MEL scripts from `dcc_plugin\src\mel\` to `dcc_plugin\maya_module\scripts\`.

`RunMayaBatchRegression.bat` defaults to `C:\Program Files\Autodesk\Maya2022\bin\mayapy.exe`. If Maya is installed elsewhere, set `MAYA_PYTHON_EXE_OVERRIDE` before running it.

`RunMayaInteractiveValidation.bat` defaults to `C:\Program Files\Autodesk\Maya2022\bin\maya.exe`. It launches Maya with `MAYA_SKIP_USERSETUP_PY=1`, loads the built plugin, sources the DMX MEL scripts from `dcc_plugin\src\mel\`, verifies the expected MEL entrypoints exist, and opens a small validation window with buttons for the import/export option boxes. If Maya is installed elsewhere, set `MAYA_EXE_OVERRIDE` before running it.

`QueryMayaValidationEnv.bat` / `tools\QueryMayaValidationEnv.ps1` query the host validation environment and write a Markdown report to `dcc_plugin\build\maya_validation_env_report.md`.

`mayaDmxWorkflow` now executes both single exports and batch exports. Export presets still use Maya optionVars, while batch manifests are stored as files under the Maya user prefs directory to avoid DAG path corruption.

Workflow cleanup helpers:

- `-listLegacyBatches` lists legacy optionVar-backed batch manifests that have not been migrated yet.
- `-migrateLegacyBatches` writes legacy optionVar-backed batch manifests to the file-backed workflow directory and removes the old optionVars.
- `-cleanupBatchStorage` removes invalid `.batch` files with undecodable names and prunes legacy batch optionVars when the file-backed manifest already exists.

The module also now ships a minimal Alembic-style MEL export UI layer. Source files are managed under:

- [src/mel/performDmxExport.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/performDmxExport.mel)
- [src/mel/performDmxImport.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/performDmxImport.mel)
- [src/mel/doDmxImportArgList.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/doDmxImportArgList.mel)
- [src/mel/doDmxExportArgList.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/doDmxExportArgList.mel)
- [src/mel/DmxCreateUI.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/DmxCreateUI.mel)
- [src/mel/mayaDmxTranslatorExport.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/mayaDmxTranslatorExport.mel)
- [src/mel/mayaDmxTranslatorImport.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/mayaDmxTranslatorImport.mel)

`InstallPluginModuleToMaya.bat` copies them into `maya_module/scripts/` during deployment.

`Valve DMX Export` now registers `mayaDmxTranslatorExport` as its translator options script, so the export settings also appear in Maya's file type specific options area. `Valve DMX Import` also registers `mayaDmxTranslatorImport` there, with working `importMaterials / importSkin / importDeltaStates` toggles that are consumed by the importer.

## Current Import Scope

The importer now supports a minimal DMX subset:

- top-level `DmeModel`, `DmeDag`, `DmeJoint`
- inline or referenced `DmeTransform`
- inline `DmeMesh` with `bindState/baseStates`, `positions`, `positionsIndices`, `normals`, `normalsIndices`, `textureCoordinates`, `textureCoordinatesIndices`, `jointCount`, `jointWeights`, `jointIndices`, `faceSets`, `faces`
- `name`, `position`, `orientation`, `children`, `jointList`, `upAxis`

Current limitations:

- supports `.dmx`, `.dmxb`, and `.dmxbin`, with binary import covering the same minimal attribute subset as text
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
- supports `exportMetadata=0` to strip Maya-specific material, UV/tangent, and deformer metadata for smaller exports

Current limitations:

- text DMX remains the default export mode
- binary DMX export currently writes only the minimal attribute subset used by this plugin
- disabling metadata also disables exported face-set material metadata plus Maya-specific UV/tangent and deformer reconstruction hints
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
- `dcc_plugin/samples/MostComplexSampleSet/chr_mesh.dmx`
- `dcc_plugin/samples/ctm_fbi/ctm_fbi.smd`
- `dcc_plugin/samples/ctm_fbi/ctm_fbi_physics.smd`
- `dcc_plugin/samples/ctm_fbi/ctm_fbi_w_ct_base_glove.smd`
- `dcc_plugin/samples/ctm_fbi/ctm_fbi_anims/default.smd`
- `dcc_plugin/samples/ctm_fbi/ctm_fbi_anims/ragdoll.smd`
- `dcc_plugin/samples/ctm_fbi/ctm_fbi_anims/rom_skin.smd`
- `dcc_plugin/samples/ctm_fbi/ctm_fbi_anims/shield_deploy.smd`

Regression inputs can also live under subdirectories of `dcc_plugin/samples/`. Current regression scripts normalize nested sample names like `MostComplexSampleSet/chr_mesh` into flat output filenames such as `MostComplexSampleSet__chr_mesh.roundtrip.dmx`.

## Layout

- `cmake/` - Maya SDK detection and plugin target helpers
- `maya_module/` - local module payload staged for Maya installation
- `samples/` - small text/binary DMX files for manual plugin testing
- `tools/` - small command-line helpers for sample conversion and regression prep, including Maya standalone regression
- `src/common/` - shared utilities and diagnostics
- `src/importer/` - DMX importer translator
- `src/exporter/` - DMX exporter translator
- `src/mel/` - MEL source files for option boxes and workflow UI helpers
- `src/workflow/` - workflow-layer preset, batch manifest, and export execution management
- `src/plugin/` - plugin entry point and translator registration
