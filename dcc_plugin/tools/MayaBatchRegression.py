import argparse
import os
import sys

import maya.standalone


def run_case(cmds, plugin_path, sample_dir, output_dir, case_name):
    input_path = os.path.join(sample_dir, f"{case_name}.dmx")
    exported_text = os.path.join(output_dir, f"{case_name}.maya_export.dmx")
    exported_binary = os.path.join(output_dir, f"{case_name}.maya_export.dmxb")

    if not os.path.isfile(input_path):
        raise RuntimeError(f"Missing sample file: {input_path}")

    cmds.file(new=True, force=True)
    if not cmds.pluginInfo(plugin_path, query=True, loaded=True):
        cmds.loadPlugin(plugin_path)

    cmds.file(input_path, i=True, type="Valve DMX Import", ignoreVersion=True, ra=True, mergeNamespacesOnClash=False)
    cmds.file(rename=exported_text)
    cmds.file(force=True, exportAll=True, type="Valve DMX Export")

    cmds.file(rename=exported_binary)
    cmds.file(force=True, options="binary=1", exportAll=True, type="Valve DMX Export")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--plugin", required=True)
    parser.add_argument("--samples", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--cases", nargs="+", required=True)
    args = parser.parse_args()

    os.makedirs(args.output, exist_ok=True)

    maya.standalone.initialize(name="python")
    try:
        import maya.cmds as cmds

        for case_name in args.cases:
            run_case(cmds, args.plugin, args.samples, args.output, case_name)
    finally:
        maya.standalone.uninitialize()


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        sys.stderr.write(f"{exc}\n")
        sys.exit(1)
