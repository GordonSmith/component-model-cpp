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
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading

# Force UTF-8 encoding for stdout on Windows
if sys.platform == 'win32':
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

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

# Platform-specific symbols
if sys.platform == 'win32':
    CHECKMARK = "OK"
    CROSSMARK = "FAIL"
    SKIPMARK = "SKIP"
else:
    CHECKMARK = "✓"
    CROSSMARK = "✗"
    SKIPMARK = "⊘"

def find_wit_files(directory: Path) -> List[Path]:
    """Recursively find all .wit files in a directory"""
    return sorted(directory.rglob("*.wit"))

def generate_cmake_file(output_prefix: Path, generated_files: List[Path], project_name: str, unique_target_prefix: str = "") -> None:
    """
    Generate a CMakeLists.txt file for testing stub compilation
    
    Args:
        output_prefix: Path prefix for generated files
        generated_files: List of generated file paths
        project_name: Name for the CMake project
        unique_target_prefix: Unique prefix for targets to avoid name collisions (e.g., "wasi-cli-deps-clocks-world")
    """
    # Place CMakeLists.txt in the same directory as the generated files
    cmake_path = generated_files[0].parent / "CMakeLists.txt"
    
    # Collect source files (cpp)
    sources = [f.name for f in generated_files if f.suffix == ".cpp"]
    
    # Collect header files (hpp)
    headers = [f.name for f in generated_files if f.suffix == ".hpp"]
    
    if not sources:
        # No sources to compile, skip CMake generation
        return
    
    # Create unique target name using the prefix if provided
    target_base = f"{unique_target_prefix}-{project_name}" if unique_target_prefix else project_name
    
    # Generate CMakeLists.txt content
    cmake_content = f"""# Generated CMakeLists.txt for testing {project_name} stub compilation
# NOTE: This CMakeLists.txt is generated for compilation testing purposes.
# Files ending in _wamr.cpp require the WAMR (WebAssembly Micro Runtime) headers.
# To build successfully, ensure WAMR is installed or available in your include path.

cmake_minimum_required(VERSION 3.10)
project({project_name}-stub-test)

# Note: When built as part of the root CMakeLists.txt, include paths are inherited.
# When built standalone, you need to set CMCPP_INCLUDE_DIR or install cmcpp package.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only search for cmcpp if not already configured (standalone build)
if(NOT DEFINED CMCPP_INCLUDE_DIR AND NOT TARGET cmcpp)
    find_package(cmcpp QUIET)
    if(NOT cmcpp_FOUND)
        # Use local include directory (for standalone testing without installation)
        # Calculate path based on directory depth
        set(CMCPP_INCLUDE_DIR "${{CMAKE_CURRENT_SOURCE_DIR}}/../../../../include")
        if(NOT EXISTS "${{CMCPP_INCLUDE_DIR}}/cmcpp.hpp")
            message(FATAL_ERROR "cmcpp headers not found. Set CMCPP_INCLUDE_DIR or install cmcpp package.")
        endif()
        message(STATUS "Using local cmcpp from: ${{CMCPP_INCLUDE_DIR}}")
        include_directories(${{CMCPP_INCLUDE_DIR}})
    endif()
endif()

# Suppress narrowing conversion warnings for WebAssembly 32-bit ABI
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/wd4244")
    add_compile_options(
        /wd4244  # conversion from 'type1' to 'type2', possible loss of data
        /wd4267  # conversion from 'size_t' to 'type', possible loss of data
        /wd4305  # truncation from 'type1' to 'type2'
        /wd4309  # truncation of constant value
    )
endif()

# Source files (may include _wamr.cpp which requires WAMR headers)
set(STUB_SOURCES
    {chr(10).join('    ' + s for s in sources)}
)

# Create a static library from the generated stub
# NOTE: Compilation may fail if WAMR headers are not available
add_library({target_base}-stub STATIC
    ${{STUB_SOURCES}}
)

if(TARGET cmcpp::cmcpp)
    target_link_libraries({target_base}-stub PRIVATE cmcpp::cmcpp)
else()
    target_include_directories({target_base}-stub PRIVATE ${{CMCPP_INCLUDE_DIR}})
endif()

target_include_directories({target_base}-stub
    PRIVATE ${{CMAKE_CURRENT_SOURCE_DIR}}
)

# Optional: Add a test executable that just includes the headers
add_executable({target_base}-stub-test
    {sources[0] if sources else 'test.cpp'}
)

target_link_libraries({target_base}-stub-test
    PRIVATE {target_base}-stub
)

if(NOT TARGET cmcpp::cmcpp)
    target_include_directories({target_base}-stub-test PRIVATE ${{CMCPP_INCLUDE_DIR}})
endif()

# Headers for reference (not compiled directly)
# {chr(10).join('# ' + h for h in headers)}

# To use this CMakeLists.txt:
# 1. Ensure cmcpp headers are available
# 2. For _wamr.cpp files: Install WAMR or set WAMR include paths
# 3. Configure: cmake -S . -B build
# 4. Build: cmake --build build
"""
    
    # Write CMakeLists.txt
    cmake_path.write_text(cmake_content, encoding='utf-8')

def generate_root_cmake_file(output_dir: Path, stub_dirs: List[Path]) -> None:
    """
    Generate a root CMakeLists.txt file that adds all the stub subdirectories
    """
    root_cmake_path = output_dir / "CMakeLists.txt"
    
    # Sort stub directories for deterministic output
    stub_dirs = sorted(stub_dirs)
    
    # Generate CMakeLists.txt content
    cmake_content = f"""# Generated root CMakeLists.txt for all WIT test stubs
# This file adds all individual stub subdirectories for compilation testing

cmake_minimum_required(VERSION 3.10)
project(wit-test-stubs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try to find cmcpp package, fallback to local include
find_package(cmcpp QUIET)
if(NOT cmcpp_FOUND)
    # Use local include directory (for testing without installation)
    # From: build/test/generated_stubs/ -> ../../../include
    set(CMCPP_INCLUDE_DIR "${{CMAKE_CURRENT_SOURCE_DIR}}/../../../include")
    if(NOT EXISTS "${{CMCPP_INCLUDE_DIR}}/cmcpp.hpp")
        message(FATAL_ERROR "cmcpp headers not found. Set CMCPP_INCLUDE_DIR or install cmcpp package.")
    endif()
    message(STATUS "Using local cmcpp from: ${{CMCPP_INCLUDE_DIR}}")
    
    # Set include directories for all subdirectories
    # This way each stub doesn't need to calculate its own path
    include_directories(${{CMCPP_INCLUDE_DIR}})
endif()

# Find WAMR (required for _wamr.cpp files)
# Attempt to locate common vcpkg install locations first, then fall back to find_path
set(_wamr_hint_candidates "")

if(DEFINED VCPKG_TARGET_TRIPLET)
    list(APPEND _wamr_hint_candidates "${{CMAKE_CURRENT_SOURCE_DIR}}/../../vcpkg_installed/${{VCPKG_TARGET_TRIPLET}}/include")
endif()

if(DEFINED VCPKG_HOST_TRIPLET)
    list(APPEND _wamr_hint_candidates "${{CMAKE_CURRENT_SOURCE_DIR}}/../../vcpkg_installed/${{VCPKG_HOST_TRIPLET}}/include")
endif()

list(APPEND _wamr_hint_candidates
    "${{CMAKE_CURRENT_SOURCE_DIR}}/../../vcpkg_installed/x64-windows/include"
    "${{CMAKE_CURRENT_SOURCE_DIR}}/../../vcpkg_installed/x64-windows-static/include"
    "${{CMAKE_CURRENT_SOURCE_DIR}}/../../vcpkg_installed/x64-linux/include"
    "${{CMAKE_CURRENT_SOURCE_DIR}}/../../vcpkg_installed/x64-osx/include"
)

list(REMOVE_DUPLICATES _wamr_hint_candidates)

set(WAMR_INCLUDE_HINT "")
foreach(_hint IN LISTS _wamr_hint_candidates)
    if(NOT WAMR_INCLUDE_HINT AND EXISTS "${{_hint}}/wasm_export.h")
        set(WAMR_INCLUDE_HINT "${{_hint}}")
    endif()
endforeach()

if(WAMR_INCLUDE_HINT)
    message(STATUS "Using WAMR from: ${{WAMR_INCLUDE_HINT}}")
    include_directories(${{WAMR_INCLUDE_HINT}})
else()
    # Try to find via CMake
    find_path(WAMR_INCLUDE_DIR wasm_export.h)
    if(WAMR_INCLUDE_DIR)
        message(STATUS "Found WAMR at: ${{WAMR_INCLUDE_DIR}}")
        include_directories(${{WAMR_INCLUDE_DIR}})
    else()
        message(WARNING "WAMR headers not found. Stubs requiring wasm_export.h will fail to compile.")
    endif()
endif()

# Suppress narrowing conversion warnings for WebAssembly 32-bit ABI (applies to all subdirectories)
if(MSVC)
    add_compile_options(
        /wd4244  # conversion from 'type1' to 'type2', possible loss of data
        /wd4267  # conversion from 'size_t' to 'type', possible loss of data
        /wd4305  # truncation from 'type1' to 'type2'
        /wd4309  # truncation of constant value
    )
endif()

# Add all stub subdirectories
message(STATUS "Adding WIT test stub subdirectories...")

"""
    
    # Add each subdirectory
    for stub_dir in stub_dirs:
        # Get relative path from output_dir
        try:
            rel_path = stub_dir.relative_to(output_dir)
            rel_path_str = rel_path.as_posix()
            cmake_content += f'add_subdirectory("{rel_path_str}")\n'
        except ValueError:
            # If not relative, skip
            pass
    
    cmake_content += f"""
# Summary
message(STATUS "Added {len(stub_dirs)} WIT test stub directories")
"""
    
    # Write CMakeLists.txt
    root_cmake_path.write_text(cmake_content, encoding='utf-8')

def generate_stub(wit_file: Path, output_prefix: Path, codegen_tool: Path, verbose: bool = False, generate_cmake: bool = True, unique_target_prefix: str = "") -> Tuple[bool, str]:
    """
    Generate stub files for a single WIT file
    Returns: (success: bool, message: str)
    """
    try:
        # Run wit-codegen with increased timeout for complex files
        # (some large WASI files take longer to process, especially in parallel)
        result = subprocess.run(
            [str(codegen_tool), str(wit_file), str(output_prefix)],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        if result.returncode != 0:
            error_msg = result.stderr.strip() if result.stderr else "Unknown error"
            return False, error_msg
        
        # Check if files were generated (wit-codegen generates _wamr.cpp and _wamr.hpp)
        possible_files = [
            output_prefix.with_suffix(".hpp"),                    # main header
            Path(str(output_prefix) + "_wamr.hpp"),              # WAMR header
            Path(str(output_prefix) + "_wamr.cpp"),              # WAMR implementation
        ]
        
        files_exist = [f for f in possible_files if f.exists()]
        
        if not files_exist:
            return False, "No output files generated"
        
        # Generate CMakeLists.txt for this stub
        if generate_cmake and files_exist:
            project_name = output_prefix.name
            generate_cmake_file(output_prefix, files_exist, project_name, unique_target_prefix)
        
        if verbose:
            return True, f"Generated: {', '.join(f.name for f in files_exist)}"
        
        return True, ""
        
    except subprocess.TimeoutExpired:
        return False, "Timeout"
    except Exception as e:
        return False, str(e)

def process_wit_file(args_tuple):
    """
    Process a single WIT file (wrapper for parallel execution)
    Returns: (index, wit_file, success, message)
    """
    index, wit_file, test_dir, output_dir, codegen_tool, verbose, generate_cmake = args_tuple
    
    # Get relative path for better organization
    rel_path = wit_file.relative_to(test_dir)
    
    # Create subdirectory for each stub to ensure individual CMakeLists.txt
    stub_name = rel_path.stem
    stub_dir = output_dir / rel_path.parent / stub_name
    stub_dir.mkdir(parents=True, exist_ok=True)
    
    # Output files go into the stub-specific directory
    output_prefix = stub_dir / stub_name
    
    # Create a unique target prefix to avoid name collisions
    # Convert path separators to dashes and remove any special characters
    unique_prefix = str(rel_path.parent / stub_name).replace("/", "-").replace("\\", "-")
    
    # Generate stub
    success, message = generate_stub(
        wit_file, 
        output_prefix, 
        codegen_tool, 
        verbose,
        generate_cmake=generate_cmake,
        unique_target_prefix=unique_prefix
    )
    
    return (index, rel_path, success, message)

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
    parser.add_argument(
        "--no-cmake",
        action="store_true",
        help="Skip CMakeLists.txt generation for each stub"
    )
    parser.add_argument(
        "-j", "--jobs",
        type=int,
        default=None,
        help="Number of parallel jobs (default: CPU count)"
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
    
    # Determine number of parallel jobs
    max_workers = args.jobs if args.jobs else os.cpu_count()
    
    # Print header
    print("WIT Grammar Test Stub Generator")
    print("=" * 50)
    print(f"Test directory:   {test_dir}")
    print(f"Output directory: {output_dir}")
    print(f"Code generator:   {codegen_tool}")
    print(f"Parallel jobs:    {max_workers}")
    if args.filter:
        print(f"Filter:           {args.filter}")
    print(f"\nFound {len(wit_files)} WIT files\n")
    
    # Prepare arguments for parallel processing
    work_items = [
        (i + 1, wit_file, test_dir, output_dir, codegen_tool, args.verbose, not args.no_cmake)
        for i, wit_file in enumerate(wit_files)
    ]
    
    # Process files in parallel
    success_count = 0
    failure_count = 0
    skipped_count = 0
    failures = []
    generated_stub_dirs = []
    
    # Use a lock for thread-safe printing and list updates
    print_lock = threading.Lock()
    
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        # Submit all tasks
        future_to_work = {executor.submit(process_wit_file, work_item): work_item for work_item in work_items}
        
        # Process results as they complete
        for future in as_completed(future_to_work):
            index, rel_path, success, message = future.result()
            
            # Track generated stub directories
            if success:
                stub_name = rel_path.stem
                stub_dir = output_dir / rel_path.parent / stub_name
                with print_lock:
                    generated_stub_dirs.append(stub_dir)
            
            # Thread-safe printing
            with print_lock:
                prefix = f"[{index}/{len(wit_files)}]"
                print(f"{prefix} Processing: {rel_path} ... ", end="", flush=True)
                
                if success:
                    print(Colors.color(CHECKMARK, Colors.GREEN))
                    if args.verbose and message:
                        print(f"       {message}")
                    success_count += 1
                elif "No output files generated" in message:
                    print(Colors.color(f"{SKIPMARK} (no output)", Colors.YELLOW))
                    skipped_count += 1
                else:
                    print(Colors.color(CROSSMARK, Colors.RED))
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
    
    # Generate root CMakeLists.txt if we have successful generations
    if success_count > 0 and not args.no_cmake:
        print(f"\nGenerating root CMakeLists.txt...")
        generate_root_cmake_file(output_dir, generated_stub_dirs)
        print(f"{Colors.color(CHECKMARK, Colors.GREEN)} Created {output_dir / 'CMakeLists.txt'}")
    
    print(f"\n{Colors.color(CHECKMARK, Colors.GREEN)} Stub generation complete!")
    print(f"Output directory: {output_dir}")
    if not args.no_cmake:
        print(f"CMakeLists.txt files generated for each stub (use --no-cmake to disable)")
    
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
