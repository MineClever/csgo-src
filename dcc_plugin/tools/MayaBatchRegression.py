import argparse
import os
import sys

import maya.standalone

from maya_batch_regression_lib import RegressionConfig, RegressionRunner


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--plugin", dest="plugin_dmx", required=True)
    parser.add_argument("--plugin-smd")
    parser.add_argument("--samples", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--cases", nargs="+")
    parser.add_argument(
        "--config",
        default=os.path.join(os.path.dirname(__file__), "MayaBatchRegression.config.json"),
    )
    return parser.parse_args()


def main():
    args = parse_args()
    args.plugin_dmx = os.path.abspath(args.plugin_dmx)
    if args.plugin_smd:
        args.plugin_smd = os.path.abspath(args.plugin_smd)
    args.samples = os.path.abspath(args.samples)
    args.output = os.path.abspath(args.output)
    args.config = os.path.abspath(args.config)
    os.makedirs(args.output, exist_ok=True)

    config = RegressionConfig(args.config)
    case_names = args.cases or config.default_cases
    plugin_paths_by_format = {
        "dmx": args.plugin_dmx,
    }
    if args.plugin_smd:
        plugin_paths_by_format["smd"] = args.plugin_smd

    maya.standalone.initialize(name="python")
    try:
        import maya.cmds as cmds

        runner = RegressionRunner(
            cmds,
            config,
            plugin_paths_by_format,
            args.samples,
            args.output,
        )
        runner.run_cases(case_names)
    finally:
        maya.standalone.uninitialize()


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        sys.stderr.write(f"{exc}\n")
        sys.exit(1)
