import argparse
import os
import runpy

import maya.cmds as cmds
import maya.mel as mel
import maya.utils


def _normalize(path):
    return path.replace("\\", "/")


def _source_mel(script_dir, file_name):
    mel.eval('source "{0}/{1}"'.format(_normalize(script_dir), file_name))


def _ensure_plugin_loaded(plugin_path):
    if not cmds.pluginInfo(plugin_path, query=True, loaded=True):
        cmds.loadPlugin(plugin_path)


def _ensure_proc(name):
    if not mel.eval('exists "{0}"'.format(name)):
        raise RuntimeError("Missing MEL proc: {0}".format(name))


def _build_status_lines(plugin_path, script_dir):
    return [
        "maya_dmx interactive validation ready",
        "plugin: {0}".format(_normalize(plugin_path)),
        "scripts: {0}".format(_normalize(script_dir)),
        "checks:",
        "  1. Use 'Valve DMX Import/Export' in Maya file dialogs.",
        "  2. Run MayaDmxShowImportOptions() and verify import option box layout/commit.",
        "  3. Run MayaDmxShowExportSelectionOptions() and MayaDmxShowExportAllOptions().",
        "  4. Confirm file type specific options match option box state.",
    ]


def _show_validation_window(plugin_path, script_dir):
    window_name = "mayaDmxInteractiveValidationWindow"
    if cmds.window(window_name, exists=True):
        cmds.deleteUI(window_name)

    window = cmds.window(window_name, title="Maya DMX Interactive Validation", sizeable=True, widthHeight=(720, 420))
    cmds.columnLayout(adjustableColumn=True, rowSpacing=8)
    for line in _build_status_lines(plugin_path, script_dir):
        cmds.text(label=line, align="left")

    cmds.separator(style="in")
    cmds.button(label="Open Import Options", command=lambda *_: mel.eval("MayaDmxShowImportOptions();"))
    cmds.button(label="Open Export Selection Options", command=lambda *_: mel.eval("MayaDmxShowExportSelectionOptions();"))
    cmds.button(label="Open Export All Options", command=lambda *_: mel.eval("MayaDmxShowExportAllOptions();"))
    cmds.button(label="Print Validation Paths", command=lambda *_: print("\n".join(_build_status_lines(plugin_path, script_dir))))
    cmds.showWindow(window)


def bootstrap(plugin_path, script_dir):
    plugin_path = os.path.abspath(plugin_path)
    script_dir = os.path.abspath(script_dir)

    cmds.flushUndo()
    _ensure_plugin_loaded(plugin_path)

    _source_mel(script_dir, "performDmxImport.mel")
    _source_mel(script_dir, "doDmxImportArgList.mel")
    _source_mel(script_dir, "performDmxExport.mel")
    _source_mel(script_dir, "doDmxExportArgList.mel")
    _source_mel(script_dir, "DmxCreateUI.mel")
    _source_mel(script_dir, "mayaDmxTranslatorImport.mel")
    _source_mel(script_dir, "mayaDmxTranslatorExport.mel")

    for proc_name in (
        "MayaDmxShowImportOptions",
        "MayaDmxShowExportSelectionOptions",
        "MayaDmxShowExportAllOptions",
        "mayaDmxTranslatorImport",
        "mayaDmxTranslatorExport",
    ):
        _ensure_proc(proc_name)

    print("\n".join(_build_status_lines(plugin_path, script_dir)))
    maya.utils.executeDeferred(_show_validation_window, plugin_path, script_dir)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--plugin", required=True)
    parser.add_argument("--scripts", required=True)
    args = parser.parse_args()
    bootstrap(args.plugin, args.scripts)


if __name__ == "__main__":
    main()
