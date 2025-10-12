#!/usr/bin/env python3
"""
Selectively compile generated WIT stubs to validate code generation.
Allows testing subsets of files to incrementally fix issues.
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path
from typing import List, Tuple, Optional

# ANSI color codes
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    CYAN = '\033[0;36m'
    NC = '\033[0m'  # No Color
    
    @staticmethod
    def supports_color():
        return sys.stdout.isatty()
    
    @classmethod
    def color(cls, text: str, color_code: str) -> str:
        if cls.supports_color():
            return f"{color_code}{text}{cls.NC}"
        return text

def compile_stub(stub_name: str, stub_dir: Path, include_dir: Path, verbose: bool = False) -> Tuple[bool, str]:
    """
    Compile a single stub file.
    Returns: (success: bool, error_message: str)
    """
    stub_cpp = stub_dir / f"{stub_name}_wamr.cpp"
    
    if not stub_cpp.exists():
        return False, f"File not found: {stub_cpp.name}"
    
    # Find vcpkg installed directory for WAMR headers
    # Try relative to stub_dir first (most reliable)
    vcpkg_installed = stub_dir.parent.parent / "vcpkg_installed"
    if not vcpkg_installed.exists():
        # Try relative to include_dir
        vcpkg_installed = include_dir.parent / "build" / "vcpkg_installed"
    if not vcpkg_installed.exists():
        vcpkg_installed = include_dir.parent / "vcpkg_installed"
    
    # Build compile command
    cmd = [
        "g++",
        "-std=c++20",
        "-c",
        str(stub_cpp),
        f"-I{include_dir}",
        f"-I{stub_dir}",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-o", f"/tmp/{stub_name}_wamr.o",
    ]
    
    # Add WAMR includes if vcpkg_installed exists
    if vcpkg_installed.exists():
        # Try to find the triplet include directory
        for triplet_dir in sorted(vcpkg_installed.iterdir()):
            if triplet_dir.is_dir() and not triplet_dir.name.startswith('.'):
                triplet_include = triplet_dir / "include"
                if triplet_include.exists():
                    cmd.insert(4, f"-I{triplet_include}")
                    if verbose:
                        print(f"  Using WAMR includes: {triplet_include}")
                    break
    elif verbose:
        print(f"  Warning: vcpkg_installed not found at {vcpkg_installed}")
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=30
        )
        
        if result.returncode == 0:
            return True, ""
        else:
            # Extract relevant error lines
            errors = []
            for line in result.stderr.split('\n'):
                if 'error:' in line or 'note:' in line:
                    errors.append(line.strip())
            
            error_msg = '\n'.join(errors[:10])  # Limit to first 10 errors
            if len(errors) > 10:
                error_msg += f"\n... and {len(errors) - 10} more errors"
            
            return False, error_msg
            
    except subprocess.TimeoutExpired:
        return False, "Compilation timeout"
    except Exception as e:
        return False, str(e)

def main():
    parser = argparse.ArgumentParser(
        description="Selectively compile generated WIT stubs",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Test only basic types
  %(prog)s -g basic
  
  # Test specific files
  %(prog)s floats integers strings
  
  # Test with verbose output
  %(prog)s -v -g composite
  
  # Test everything except async
  %(prog)s -g all --exclude async
  
  # List available groups
  %(prog)s --list-groups
"""
    )
    
    parser.add_argument(
        "stubs",
        nargs="*",
        help="Specific stub names to test (e.g., floats integers)"
    )
    parser.add_argument(
        "-g", "--group",
        choices=["all", "basic", "collections", "composite", "options", "functions", "resources", "async"],
        help="Test a predefined group of stubs"
    )
    parser.add_argument(
        "--exclude",
        choices=["basic", "collections", "composite", "options", "functions", "resources", "async"],
        help="Exclude a group when using --group all"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Show compilation errors"
    )
    parser.add_argument(
        "-d", "--stub-dir",
        type=Path,
        default=Path("build/test/generated_stubs"),
        help="Directory containing generated stubs"
    )
    parser.add_argument(
        "-i", "--include-dir",
        type=Path,
        default=Path("include"),
        help="Directory containing cmcpp headers"
    )
    parser.add_argument(
        "--list-groups",
        action="store_true",
        help="List available test groups and exit"
    )
    parser.add_argument(
        "--list-stubs",
        action="store_true",
        help="List available stub files and exit"
    )
    
    args = parser.parse_args()
    
    # Define test groups
    groups = {
        "basic": ["floats", "integers", "strings", "char"],
        "collections": ["lists", "zero-size-tuple"],
        "composite": ["records", "variants", "enum-has-go-keyword", "flags"],
        "options": ["simple-option", "option-result", "result-empty"],
        "functions": ["multi-return", "conventions"],
        "resources": ["resources"],
        "async": ["streams", "futures"],
    }
    
    # Resolve paths
    script_dir = Path(__file__).parent.parent
    stub_dir = (script_dir / args.stub_dir).resolve()
    include_dir = (script_dir / args.include_dir).resolve()
    
    # List groups if requested
    if args.list_groups:
        print("Available test groups:")
        for group, stubs in groups.items():
            print(f"  {Colors.color(group, Colors.CYAN):20} {', '.join(stubs)}")
        return 0
    
    # List stubs if requested
    if args.list_stubs:
        print(f"Available stubs in {stub_dir}:")
        if stub_dir.exists():
            stubs = sorted([f.stem.replace('_wamr', '') 
                          for f in stub_dir.glob('*_wamr.cpp')])
            for stub in stubs:
                print(f"  {stub}")
        else:
            print(f"  {Colors.color('Directory not found!', Colors.RED)}")
        return 0
    
    # Validation
    if not stub_dir.exists():
        print(Colors.color(f"Error: Stub directory not found: {stub_dir}", Colors.RED))
        print("Run: cmake --build build --target generate-test-stubs")
        return 1
    
    if not include_dir.exists():
        print(Colors.color(f"Error: Include directory not found: {include_dir}", Colors.RED))
        return 1
    
    # Determine which stubs to test
    stubs_to_test = []
    
    if args.stubs:
        # Explicit list provided
        stubs_to_test = args.stubs
    elif args.group:
        # Group specified
        if args.group == "all":
            for group_name, group_stubs in groups.items():
                if args.exclude and group_name == args.exclude:
                    continue
                stubs_to_test.extend(group_stubs)
        else:
            stubs_to_test = groups.get(args.group, [])
    else:
        # Default: test basic types
        stubs_to_test = groups["basic"]
        print(Colors.color("No stubs specified, testing basic group by default", Colors.YELLOW))
        print(Colors.color("Use --group or specify stub names explicitly\n", Colors.YELLOW))
    
    if not stubs_to_test:
        print(Colors.color("Error: No stubs to test!", Colors.RED))
        return 1
    
    # Print header
    print(Colors.color("WIT Stub Compilation Validation", Colors.CYAN))
    print("=" * 60)
    print(f"Stub directory:    {stub_dir}")
    print(f"Include directory: {include_dir}")
    print(f"Testing {len(stubs_to_test)} stubs\n")
    
    # Test each stub
    success_count = 0
    failure_count = 0
    not_found_count = 0
    failures = []
    
    for i, stub_name in enumerate(stubs_to_test, 1):
        prefix = f"[{i}/{len(stubs_to_test)}]"
        print(f"{prefix} {stub_name:30} ... ", end="", flush=True)
        
        success, error = compile_stub(stub_name, stub_dir, include_dir, args.verbose)
        
        if success:
            print(Colors.color("✓", Colors.GREEN))
            success_count += 1
        elif "not found" in error:
            print(Colors.color("⊘ (not generated)", Colors.YELLOW))
            not_found_count += 1
        else:
            print(Colors.color("✗", Colors.RED))
            failure_count += 1
            failures.append((stub_name, error))
            
            if args.verbose:
                print(Colors.color(f"  Errors:", Colors.RED))
                for line in error.split('\n'):
                    if line.strip():
                        print(f"    {line}")
                print()
    
    # Print summary
    print("\n" + "=" * 60)
    print("Summary:")
    print(f"  Total tested:     {len(stubs_to_test)}")
    print(Colors.color(f"  Successful:       {success_count}", Colors.GREEN))
    print(Colors.color(f"  Failed:           {failure_count}", Colors.RED))
    print(Colors.color(f"  Not generated:    {not_found_count}", Colors.YELLOW))
    
    if failures:
        print(f"\n{Colors.color('Failed stubs:', Colors.RED)}")
        for stub_name, error in failures:
            print(f"  - {stub_name}")
            if not args.verbose:
                # Show just first error line
                first_error = error.split('\n')[0] if error else "Unknown error"
                print(f"    {first_error}")
        
        if not args.verbose:
            print(f"\n{Colors.color('Run with -v to see full error output', Colors.YELLOW)}")
    
    # Success rate
    if stubs_to_test:
        success_rate = (success_count / len(stubs_to_test)) * 100
        print(f"\nSuccess rate: {success_rate:.1f}%")
    
    # Exit code: 0 if all succeeded, 1 if any failed
    return 0 if failure_count == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
