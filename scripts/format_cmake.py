#!/usr/bin/env python3
"""
@file cmake_formatter.py
@brief A utility script to recursively format CMake files.
"""

import argparse
import subprocess
import fnmatch
import os
import sys
import shutil

## @var CMAKE_FILES_PATTERNS
#  @brief Patterns for matching CMake files to format
CMAKE_FILES_PATTERNS = [
    "CMakeLists.txt",
    "*.cmake",
]

def matches_patterns(filename, patterns):
    """
    @brief Checks if a filename matches any of the given glob patterns.

    @param filename The name of the file to check.
    @param patterns A list of glob patterns (e.g., ["*.txt"]).
    @return True if the filename matches any pattern, False otherwise.
    """
    return any(fnmatch.fnmatch(filename, pat) for pat in patterns)

def find_cmake_files(root_dir, exclude_dirs):
    """
    @brief Recursively scans the root directory for CMake files, skipping excluded directories.

    @param root_dir The starting directory for the search.
    @param exclude_dirs A set of absolute paths to directories to skip.
    @return A list of absolute paths to found CMake files.
    """
    cmake_files = []
    for root, dirs, files in os.walk(root_dir):
        # Exclude directories by modifying dirs in-place
        dirs[:] = [d for d in dirs if os.path.join(root, d) not in exclude_dirs]

        for f in files:
            if matches_patterns(f, CMAKE_FILES_PATTERNS):
                cmake_files.append(os.path.join(root, f))
    return cmake_files

def run_cmake_format(files, config=None, check_only=False, dry_run=False):
    """
    @brief Runs cmake-format on a list of files.

    @param files A list of file paths to format.
    @param config Optional path to a configuration file.
    @param check_only If True, only check if files are formatted correctly without modifying.
    @param dry_run If True, show what would be formatted without making changes.
    @return A list of tuples containing (filename, exception/message) for any failures or issues.
    """
    errors = []
    needs_formatting = []
    
    for f in files:
        if dry_run:
            print(f"[would format] {f}")
            continue
            
        if check_only:
            cmd = ["cmake-format", "--check", f]
        else:
            cmd = ["cmake-format", "-i", f]
            
        if config:
            cmd.extend(["--config", config])

        try:
            result = subprocess.run(cmd, check=False, capture_output=True, text=True)
            
            if check_only:
                if result.returncode != 0:
                    needs_formatting.append(f)
                    print(f"[needs formatting] {f}")
                else:
                    print(f"[ok] {f}")
            else:
                if result.returncode == 0:
                    print(f"[formatted] {f}")
                else:
                    errors.append((f, f"Formatting failed: {result.stderr}"))
                    
        except FileNotFoundError:
            print("Error: 'cmake-format' executable not found in PATH.")
            sys.exit(1)
    
    if check_only and needs_formatting:
        return needs_formatting
            
    return errors

def main():
    """
    @brief Main entry point for the script.
    """
    # check if cmake-format is installed
    if not shutil.which("cmake-format"):
        print("Error: 'cmake-format' is not installed or not in your PATH.")
        print("Try installing it via: pip install cmakelang")
        sys.exit(1)

    parser = argparse.ArgumentParser(
        description="Format all CMake files recursively in the current directory.",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog="""
Examples:
  # Format everything in current directory
  ./format_cmake.py

  # Exclude build and third_party directories
  ./format_cmake.py --exclude build third_party

  # Check formatting without modifying (for CI/CD)
  ./format_cmake.py --check --exclude build

  # Dry-run to preview what would be formatted
  ./format_cmake.py --dry-run --exclude build third_party

  # Use a specific config file
  ./format_cmake.py --config .cmake-format.yaml
        """
    )
    
    parser.add_argument(
        "--exclude",
        nargs="*",
        default=[],
        help="Directories to exclude (relative paths).",
    )
    parser.add_argument(
        "--config",
        type=str,
        default=None,
        help="Optional path to cmake-format config file (.cmake-format.yaml).",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check if files are formatted correctly without modifying them. Exit code 1 if any need formatting.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show which files would be formatted without actually formatting them.",
    )

    args = parser.parse_args()

    # Normalize excluded paths to absolute paths for reliable comparison
    exclude_dirs = {os.path.abspath(e) for e in args.exclude}

    root = os.getcwd()
    print(f"Scanning directory: {root}")
    
    cmake_files = find_cmake_files(root, exclude_dirs)

    if not cmake_files:
        print("No CMake files found.")
        sys.exit(0)

    print(f"Found {len(cmake_files)} CMake files.")

    if args.dry_run:
        print("\n--- DRY RUN MODE ---")
        
    result = run_cmake_format(cmake_files, config=args.config, 
                               check_only=args.check, dry_run=args.dry_run)

    if args.check:
        if result:
            print(f"\n❌ {len(result)} file(s) need formatting:")
            for f in result:
                print(f"  {f}")
            sys.exit(1)
        else:
            print("\n✓ All files are properly formatted.")
    elif args.dry_run:
        print("\n--- DRY RUN COMPLETE ---")
    else:
        if result:
            print("\nErrors occurred during formatting:")
            for f, e in result:
                print(f"  {f}: {e}")
            sys.exit(1)
        print("\n✓ Formatting complete.")

if __name__ == "__main__":
    main()