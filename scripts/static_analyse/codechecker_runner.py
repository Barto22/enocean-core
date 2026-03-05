import argparse
import json
import re
import os
import subprocess
import sys
from pathlib import Path
import shutil
from typing import List


def parse_arguments():
    """Parses command line arguments."""
    parser = argparse.ArgumentParser(
        description="Prepare compile_commands.json for CodeChecker and run analysis."
    )
    parser.add_argument(
        "codechecker_config_dir",
        type=str,
        help="Path to the directory containing CodeChecker config files."
    )
    parser.add_argument(
        "build_dir",
        type=str,
        help="Path to the CMake build directory containing compile_commands.json."
    )
    return parser.parse_args()


def modify_compilation_database(input_path: Path, output_path: Path):
    """
    Reads the compilation database, applies regex replacements,
    and writes the modified database.
    """
    print("\nModifying compilation commands...")

    if not input_path.exists():
        print(f"Error: Input file not found at {input_path}")
        sys.exit(1)

    try:
        with open(input_path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except Exception as e:
        print(f"Error reading {input_path}: {e}")
        sys.exit(1)

    replace_modules_ts = re.compile(r"-fmodules-ts")
    remove_deps_format = re.compile(r"-fdeps-format=p1689r5")
    remove_module_mapper = re.compile(r"-fmodule-mapper=[^\s]*")

    modified_data = []

    for entry in data:
        command = entry.get("command")
        if command:
            command = replace_modules_ts.sub("-fmodules", command)
            command = remove_deps_format.sub("", command)
            command = remove_module_mapper.sub("", command)
            command = re.sub(r"\s+", " ", command).strip()
            entry["command"] = command

        modified_data.append(entry)

    try:
        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(modified_data, f, indent=4)
        print(f"Successfully created modified compilation database: {output_path}")
    except Exception as e:
        print(f"Error writing to {output_path}: {e}")
        sys.exit(1)


def clean_directory(path: Path, description: str):
    """Removes a directory if it exists."""
    if path.exists():
        try:
            shutil.rmtree(path)
            print(f"Removed existing {description}: {path}")
        except OSError as e:
            print(f"Error removing {description} {path}: {e}")
            sys.exit(1)


def is_codechecker_success(cmd: List[str], returncode: int) -> bool:
    """
    Determines whether a CodeChecker command finished successfully
    based on its exit code and subcommand.
    """
    if len(cmd) < 2:
        return returncode == 0

    subcommand = cmd[1]

    # CodeChecker analyze:
    #   0 = success, no reports
    #   3 = success, reports produced
    if subcommand == "analyze":
        return returncode in (0, 3)

    # CodeChecker parse --export html may return non-zero even on success
    if subcommand == "parse" and "--export" in cmd and "html" in cmd:
        return returncode in (0, 1, 2)

    return returncode == 0


def run_subprocess(cmd: List[str]):
    """Executes a subprocess command with CodeChecker-aware exit handling."""
    cmd_str = " ".join(cmd)
    print(f"\nExecuting command: {cmd_str}")

    try:
        result = subprocess.run(
            cmd,
            check=False,
            capture_output=True,
            text=True,
        )

        if is_codechecker_success(cmd, result.returncode):
            print("Command executed successfully.")
            return

        print(f"Error: Command failed with exit code {result.returncode}")

        if result.stdout:
            print(f"STDOUT (last 500 chars):\n{result.stdout[-500:]}")
        if result.stderr:
            print(f"STDERR:\n{result.stderr}")

        sys.exit(result.returncode)

    except FileNotFoundError:
        print("Error: CodeChecker command not found. Make sure CodeChecker is installed and in your PATH.")
        sys.exit(1)
    except Exception as e:
        print(f"An unexpected error occurred during command execution: {e}")
        sys.exit(1)


def main():
    args = parse_arguments()

    config_dir = Path(args.codechecker_config_dir).resolve()
    build_dir = Path(args.build_dir).resolve()

    input_json_path = build_dir / "compile_commands.json"
    output_json_path = build_dir / "compile_commands_static_analyse.json"
    reports_dir = build_dir / "reports"
    reports_html_dir = build_dir / "reports_html"

    print("--- CodeChecker Analysis Automation ---")
    print(f"Config Directory: {config_dir}")
    print(f"Build Directory:  {build_dir}")

    # 1. Modify compilation database
    modify_compilation_database(input_json_path, output_json_path)

    # 2. Change directory to config dir
    print(f"\nChanging working directory to: {config_dir}")
    try:
        os.chdir(config_dir)
    except OSError as e:
        print(f"Error changing directory: {e}")
        sys.exit(1)

    # 3. Clean output directories
    print("\nCleaning up existing reports directories...")
    clean_directory(reports_dir, "reports directory")
    clean_directory(reports_html_dir, "HTML reports directory")

    reports_dir.mkdir(parents=True, exist_ok=True)
    reports_html_dir.mkdir(parents=True, exist_ok=True)

    # 4. Run CodeChecker
    analysis_file = output_json_path.as_posix()
    reports_output = reports_dir.as_posix()
    html_output = reports_html_dir.as_posix()

    commands = [
        ["CodeChecker", "analyze", analysis_file, "--config", "clang_tidy_config.json", "--output", reports_output],
        ["CodeChecker", "analyze", analysis_file, "--config", "cppcheck_config.json", "--output", reports_output],
        ["CodeChecker", "parse", "--export", "html", "--output", html_output, reports_output],
    ]

    for cmd in commands:
        run_subprocess(cmd)

    print("\n--- Analysis Complete ---")
    print(f"Reports saved to: {reports_dir}")
    print(f"HTML Report generated at: {reports_html_dir / 'index.html'}")


if __name__ == "__main__":
    main()
