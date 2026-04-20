import argparse
import os
import subprocess
import sys
import time

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
    parser.add_argument("--case-timeout-seconds", type=int, default=300)
    parser.add_argument("--max-parallel-cases", type=int, default=0)
    parser.add_argument("--single-case-mode", action="store_true")
    return parser.parse_args()


def normalize_args(args):
    args.plugin_dmx = os.path.abspath(args.plugin_dmx)
    if args.plugin_smd:
        args.plugin_smd = os.path.abspath(args.plugin_smd)
    args.samples = os.path.abspath(args.samples)
    args.output = os.path.abspath(args.output)
    args.config = os.path.abspath(args.config)
    os.makedirs(args.output, exist_ok=True)
    return args


def build_child_command(args, case_name):
    command = [
        sys.executable,
        os.path.abspath(__file__),
        "--plugin",
        args.plugin_dmx,
        "--samples",
        args.samples,
        "--output",
        args.output,
        "--config",
        args.config,
        "--case-timeout-seconds",
        str(args.case_timeout_seconds),
        "--single-case-mode",
        "--cases",
        case_name,
    ]
    if args.plugin_smd:
        command.extend(["--plugin-smd", args.plugin_smd])
    return command


def make_case_log_path(output_dir, case_name):
    safe_name = case_name.replace("\\", "__").replace("/", "__").replace(":", "_")
    return os.path.join(output_dir, f"{safe_name}.case.log")


def kill_process_tree(process):
    subprocess.run(
        ["taskkill", "/PID", str(process.pid), "/T", "/F"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )


def finalize_case_process(case_state):
    case_state["log_file"].close()
    with open(case_state["log_path"], "r", encoding="utf-8", errors="replace") as log_file:
        return log_file.read()


def run_cases_with_timeout(args, case_names):
    cpu_count = os.cpu_count() or 1
    max_parallel_cases = args.max_parallel_cases or max(1, cpu_count // 2)
    if max_parallel_cases <= 0:
        raise RuntimeError("max parallel cases must be greater than zero")

    sys.stdout.write(
        f"[maya_dmx] Running {len(case_names)} cases with parallelism={max_parallel_cases}, "
        f"timeout={args.case_timeout_seconds}s\n"
    )
    sys.stdout.flush()

    pending_cases = list(case_names)
    active_cases = []

    while pending_cases or active_cases:
        while pending_cases and len(active_cases) < max_parallel_cases:
            case_name = pending_cases.pop(0)
            log_path = make_case_log_path(args.output, case_name)
            log_file = open(log_path, "w", encoding="utf-8")
            process = subprocess.Popen(
                build_child_command(args, case_name),
                stdout=log_file,
                stderr=subprocess.STDOUT,
                text=True,
                env=os.environ.copy(),
            )
            active_cases.append(
                {
                    "case_name": case_name,
                    "process": process,
                    "start_time": time.monotonic(),
                    "log_path": log_path,
                    "log_file": log_file,
                }
            )

        next_active_cases = []
        for case_state in active_cases:
            process = case_state["process"]
            elapsed = time.monotonic() - case_state["start_time"]
            return_code = process.poll()
            if return_code is None and elapsed <= args.case_timeout_seconds:
                next_active_cases.append(case_state)
                continue

            if return_code is None:
                kill_process_tree(process)
                try:
                    process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    pass
                log_text = finalize_case_process(case_state)
                if log_text:
                    sys.stdout.write(log_text)
                for remaining_case in next_active_cases:
                    kill_process_tree(remaining_case["process"])
                    try:
                        remaining_case["process"].wait(timeout=15)
                    except subprocess.TimeoutExpired:
                        pass
                    finalize_case_process(remaining_case)
                raise RuntimeError(
                    f"Case '{case_state['case_name']}' exceeded the timeout limit of {args.case_timeout_seconds} seconds"
                )

            log_text = finalize_case_process(case_state)
            if log_text:
                sys.stdout.write(log_text)
            if return_code != 0:
                for remaining_case in next_active_cases:
                    kill_process_tree(remaining_case["process"])
                    try:
                        remaining_case["process"].wait(timeout=15)
                    except subprocess.TimeoutExpired:
                        pass
                    finalize_case_process(remaining_case)
                raise RuntimeError(f"Case '{case_state['case_name']}' failed with exit code {return_code}")

        active_cases = next_active_cases
        if active_cases:
            time.sleep(0.2)

    sys.stdout.flush()


def run_cases_in_process(args, case_names):
    plugin_paths_by_format = {
        "dmx": args.plugin_dmx,
    }
    if args.plugin_smd:
        plugin_paths_by_format["smd"] = args.plugin_smd

    maya.standalone.initialize(name="python")
    try:
        import maya.cmds as cmds

        config = RegressionConfig(args.config)
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


def main():
    args = normalize_args(parse_args())
    config = RegressionConfig(args.config)
    case_names = args.cases or config.default_cases
    if args.case_timeout_seconds <= 0:
        raise RuntimeError("case timeout must be greater than zero")

    if args.single_case_mode:
        run_cases_in_process(args, case_names)
        return

    run_cases_with_timeout(args, case_names)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        sys.stderr.write(f"{exc}\n")
        sys.exit(1)
