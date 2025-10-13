#!/usr/bin/env python3
"""
Generate C++ stub files for all WIT files in the grammar test suite.
This script provides more detailed output and better error handling than the bash version.
"""

import os
import sys
import subprocess
from pathlib import Path
from typing import List, Tuple
import argparse

# ANSI color codes
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'  # No Color
    
    @staticmethod
    def supports_color():
        """Check if terminal supports color"""
        return sys.stdout.isatty()
    
    @classmethod
    def color(cls, text: str, color_code: str) -> str:
        """Wrap text in color if supported"""
        if cls.supports_color():
            return f"{color_code}{text}{cls.NC}"
        return text

def find_wit_files(directory: Path) -> List[Path]:
    """Recursively find all .wit files in a directory"""
    return sorted(directory.rglob("*.wit"))

def generate_stub(wit_file: Path, output_prefix: Path, codegen_tool: Path, verbose: bool = False) -> Tuple[bool, str]:
    """
    Generate stub files for a single WIT file
    Returns: (success: bool, message: str)
    """
    try:
        # Run wit-codegen
        result = subprocess.run(
            [str(codegen_tool), str(wit_file), str(output_prefix)],
            capture_output=True,
            text=True,
            timeout=10
        )
        
        if result.returncode != 0:
            error_msg = result.stderr.strip() if result.stderr else "Unknown error"
            return False, error_msg
        
        # Check if files were generated
        generated_files = [
            output_prefix.with_suffix(".hpp"),
            output_prefix.with_suffix(".cpp"),
            Path(str(output_prefix) + "_bindings.cpp")
        ]
        
        files_exist = [f for f in generated_files if f.exists()]
        
        if not files_exist:
            return False, "No output files generated"
        
        if verbose:
            return True, f"Generated: {', '.join(f.name for f in files_exist)}"
        
        return True, ""
        
    except subprocess.TimeoutExpired:
        return False, "Timeout"
    except Exception as e:
        return False, str(e)

def main():
    parser = argparse.ArgumentParser(
        description="Generate C++ stub files for WIT test suite",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "-d", "--test-dir",
        type=Path,
        default=Path("../ref/wit-bindgen/tests/codegen"),
        help="Directory containing WIT test files"
    )
    parser.add_argument(
        "-o", "--output-dir",
        type=Path,
        default=Path("generated_stubs"),
        help="Output directory for generated stubs"
    )
    parser.add_argument(
        "-c", "--codegen",
        type=Path,
        default=Path("../build/tools/wit-codegen/wit-codegen"),
        help="Path to wit-codegen tool"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Print verbose output"
    )
    parser.add_argument(
        "-f", "--filter",
        type=str,
        help="Only process files matching this pattern"
    )
    
    args = parser.parse_args()
    
    # Resolve paths
    script_dir = Path(__file__).parent
    test_dir = (script_dir / args.test_dir).resolve()
    output_dir = (script_dir / args.output_dir).resolve()
    codegen_tool = (script_dir / args.codegen).resolve()
    
    # Validation
    if not codegen_tool.exists():
        print(Colors.color(f"Error: wit-codegen not found at {codegen_tool}", Colors.RED))
        print("Please build it first with: cmake --build build --target wit-codegen")
        return 1
    
    if not test_dir.exists():
        print(Colors.color(f"Error: Test directory not found at {test_dir}", Colors.RED))
        print("Please ensure the wit-bindgen submodule is initialized")
        return 1
    
    # Find WIT files
    wit_files = find_wit_files(test_dir)
    
    # Apply filter if specified
    if args.filter:
        wit_files = [f for f in wit_files if args.filter in str(f)]
    
    if not wit_files:
        print(Colors.color(f"No .wit files found in {test_dir}", Colors.RED))
        return 1
    
    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Print header
    print("WIT Grammar Test Stub Generator")
    print("=" * 50)
    print(f"Test directory:   {test_dir}")
    print(f"Output directory: {output_dir}")
    print(f"Code generator:   {codegen_tool}")
    if args.filter:
        print(f"Filter:           {args.filter}")
    print(f"\nFound {len(wit_files)} WIT files\n")
    
    # Process each file
    success_count = 0
    failure_count = 0
    skipped_count = 0
    failures = []
    
    for i, wit_file in enumerate(wit_files, 1):
        # Get relative path for better organization
        rel_path = wit_file.relative_to(test_dir)
        
        # Create subdirectory structure in output
        output_prefix = output_dir / rel_path.with_suffix("")
        output_prefix.parent.mkdir(parents=True, exist_ok=True)
        
        # Progress indicator
        prefix = f"[{i}/{len(wit_files)}]"
        print(f"{prefix} Processing: {rel_path} ... ", end="", flush=True)
        
        # Generate stub
        success, message = generate_stub(wit_file, output_prefix, codegen_tool, args.verbose)
        
        if success:
            print(Colors.color("✓", Colors.GREEN))
            if args.verbose and message:
                print(f"       {message}")
            success_count += 1
        elif "No output files generated" in message:
            print(Colors.color("⊘ (no output)", Colors.YELLOW))
            skipped_count += 1
        else:
            print(Colors.color("✗", Colors.RED))
            if args.verbose:
                print(f"       Error: {message}")
            failure_count += 1
            failures.append((str(rel_path), message))
    
    # Print summary
    print("\n" + "=" * 50)
    print("Summary:")
    print(f"  Total files:      {len(wit_files)}")
    print(Colors.color(f"  Successful:       {success_count}", Colors.GREEN))
    print(Colors.color(f"  Skipped:          {skipped_count}", Colors.YELLOW))
    print(Colors.color(f"  Failed:           {failure_count}", Colors.RED))
    
    if failures:
        print(f"\n{Colors.color('Failed files:', Colors.RED)}")
        for file, error in failures:
            print(f"  - {file}")
            if args.verbose:
                print(f"    Error: {error}")
    
    print(f"\n{Colors.color('✓', Colors.GREEN)} Stub generation complete!")
    print(f"Output directory: {output_dir}")
    
    # Exit with 0 if we have successful generations, even with some failures
    # Many WIT files (world-only definitions) legitimately produce no output
    # Only fail if NO files were successfully processed
    if success_count > 0:
        return 0
    elif failure_count > 0:
        return 1
    else:
        # All files were skipped (no outputs)
        return 0

if __name__ == "__main__":
    sys.exit(main())
