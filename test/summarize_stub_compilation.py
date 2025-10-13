#!/usr/bin/env python3
"""
Summarize stub compilation results from build log.
"""

import argparse
import re
import sys
from pathlib import Path


def _compat_symbol(symbol: str, fallback: str) -> str:
    """Return symbol if stdout encoding supports it, otherwise use fallback."""
    encoding = getattr(sys.stdout, "encoding", None)
    if not encoding:
        return fallback
    try:
        symbol.encode(encoding)
    except UnicodeEncodeError:
        return fallback
    return symbol


CHECK_MARK = _compat_symbol("✓", "OK")
CROSS_MARK = _compat_symbol("✗", "X")

def parse_compilation_log(log_file):
    """Parse the build log and extract compilation results."""
    
    if not log_file.exists():
        print(f"Error: Log file not found: {log_file}")
        return [], []
    
    successful = set()
    failed = set()
    currently_building = {}  # Track what's being built
    
    with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    
    # Pattern for root CMakeLists.txt build (subdirectory-based build)
    # [XX%] Building CXX object SUBDIR/CMakeFiles/TARGET-stub.dir/FILE_wamr.cpp.o
    building_pattern = r'\[\s*\d+%\] Building CXX object (.+)/CMakeFiles/(.+)-stub\.dir/(.+)_wamr\.cpp\.o'
    
    # Pattern for successful target build
    # [XX%] Built target TARGET-stub
    success_pattern = r'\[\s*\d+%\] Built target (.+)-stub'
    
    # Pattern for compilation errors
    # gmake[2]: *** [SUBDIR/CMakeFiles/TARGET-stub.dir/build.make:XX: ...] Error 1
    error_pattern = r'gmake\[\d+\]: \*\*\* \[(.+)/CMakeFiles/(.+)-stub\.dir/'
    
    # Legacy pattern for monolithic build
    # [XX/YY] Building CXX object test/CMakeFiles/test-stubs-compiled.dir/generated_stubs/NAME_wamr.cpp.o
    legacy_success_pattern = r'\[\d+/\d+\] Building CXX object test/CMakeFiles/test-stubs-compiled\.dir/generated_stubs/(\w+)_wamr\.cpp\.o'
    
    # Legacy pattern for failed compilation
    # FAILED: test/CMakeFiles/test-stubs-compiled.dir/generated_stubs/NAME_wamr.cpp.o
    legacy_failed_pattern = r'FAILED: test/CMakeFiles/test-stubs-compiled\.dir/generated_stubs/(\w+)_wamr\.cpp\.o'
    
    # Track targets being built
    for match in re.finditer(building_pattern, content):
        subdir = match.group(1)
        target = match.group(2)
        currently_building[target] = subdir
    
    # Find all successful target builds (root CMakeLists.txt pattern)
    for match in re.finditer(success_pattern, content):
        stub_name = match.group(1)
        successful.add(stub_name)
    
    # Find all failed builds (root CMakeLists.txt pattern)
    for match in re.finditer(error_pattern, content):
        target = match.group(2)
        failed.add(target)
        # Remove from successful if it was there (shouldn't happen but be safe)
        successful.discard(target)
    
    # Legacy: Find all successful compilations (monolithic build pattern)
    for match in re.finditer(legacy_success_pattern, content):
        stub_name = match.group(1)
        successful.add(stub_name)
    
    # Legacy: Find all failed compilations (monolithic build pattern)
    for match in re.finditer(legacy_failed_pattern, content):
        stub_name = match.group(1)
        failed.add(stub_name)
        # Remove from successful if it was there (shouldn't happen but be safe)
        successful.discard(stub_name)
    
    return sorted(successful), sorted(failed)

def main():
    parser = argparse.ArgumentParser(description='Summarize stub compilation results')
    parser.add_argument('--log-file', required=True, help='Path to compilation log file')
    parser.add_argument('--stub-list', help='Path to file listing expected stubs (optional)')
    
    args = parser.parse_args()
    
    log_file = Path(args.log_file)
    
    successful, failed = parse_compilation_log(log_file)
    
    total = len(successful) + len(failed)
    
    # Print summary
    print()
    print("=" * 80)
    print("Stub Compilation Summary")
    print("=" * 80)
    print()
    print(f"Total stubs attempted: {total}")
    print(f"Successful:            {len(successful)} ({100*len(successful)//total if total > 0 else 0}%)")
    print(f"Failed:                {len(failed)} ({100*len(failed)//total if total > 0 else 0}%)")
    print()
    
    if successful:
        print("Successfully compiled stubs:")
        for stub in successful:
            print(f"  {CHECK_MARK} {stub}")
        print()
    
    if failed:
        print("Failed stubs:")
        for stub in failed:
            print(f"  {CROSS_MARK} {stub}")
        print()
        print("To see detailed errors for a specific stub, run:")
        print("  ninja test/CMakeFiles/test-stubs-compiled.dir/generated_stubs/<stub-name>_wamr.cpp.o")
    else:
        print(f"All stubs compiled successfully! {CHECK_MARK}")
    
    print()
    print("=" * 80)
    print()
    
    # Return non-zero if any failed
    return 1 if failed else 0

if __name__ == '__main__':
    sys.exit(main())
