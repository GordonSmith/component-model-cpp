#!/usr/bin/env python3
"""
Validate all WIT files from wit-bindgen test suite
Generates C++ stubs and tests compilation
"""

import os
import sys
import subprocess
from pathlib import Path
import hashlib
from concurrent.futures import ThreadPoolExecutor, as_completed
import multiprocessing

# Paths
REPO_ROOT = Path(__file__).parent.parent.resolve()
WIT_BINDGEN_DIR = REPO_ROOT / "ref" / "wit-bindgen" / "tests" / "codegen"
BUILD_DIR = REPO_ROOT / "build"
GENERATED_DIR = BUILD_DIR / "test" / "generated_wit_bindgen"
WIT_CODEGEN = BUILD_DIR / "tools" / "wit-codegen" / "wit-codegen"
INCLUDE_DIR = REPO_ROOT / "include"

# Colors
GREEN = '\033[92m'
RED = '\033[91m'
YELLOW = '\033[93m'
RESET = '\033[0m'

def find_all_wit_files():
    """Find all .wit files in wit-bindgen test suite"""
    wit_files = []
    for wit_path in WIT_BINDGEN_DIR.rglob("*.wit"):
        wit_files.append(wit_path)
    return sorted(wit_files)

def get_stub_name(wit_path):
    """Generate unique stub name from WIT file path"""
    # Use relative path from codegen directory as identifier
    rel_path = wit_path.relative_to(WIT_BINDGEN_DIR)
    # Replace path separators and remove .wit extension
    name = str(rel_path).replace("/", "_").replace(".wit", "")
    return name

def generate_stub(wit_path, stub_name):
    """Generate C++ stub for a WIT file"""
    # wit-codegen appends .hpp to the output filename, so pass without extension
    output_base = GENERATED_DIR / stub_name
    
    try:
        result = subprocess.run(
            [str(WIT_CODEGEN), str(wit_path), str(output_base)],
            capture_output=True,
            text=True,
            timeout=10
        )
        if result.returncode == 0:
            # Check if the generated file exists (wit-codegen adds .hpp)
            generated_file = Path(str(output_base) + ".hpp")
            if generated_file.exists():
                return True, None
            else:
                return False, "Generation succeeded but file not found"
        else:
            return False, f"Generation failed: {result.stderr[:200]}"
    except subprocess.TimeoutExpired:
        return False, "Generation timeout"
    except Exception as e:
        return False, str(e)

def check_if_empty_stub(stub_name):
    """Check if generated stub only contains empty namespaces"""
    stub_file = GENERATED_DIR / f"{stub_name}.hpp"
    
    try:
        with open(stub_file, 'r') as f:
            content = f.read()
            # Check for the marker comment that indicates empty interfaces
            if "This WIT file contains no concrete interface definitions" in content:
                # Also verify it only has empty namespaces
                if "namespace host {}" in content and "namespace guest {}" in content:
                    return True
    except Exception:
        pass
    
    return False

def compile_stub(stub_name):
    """Compile a generated stub"""
    stub_file = GENERATED_DIR / f"{stub_name}.hpp"
    
    compile_cmd = [
        "g++",
        "-std=c++20",
        "-fsyntax-only",
        "-I", str(INCLUDE_DIR),
        "-c", str(stub_file)
    ]
    
    try:
        result = subprocess.run(
            compile_cmd,
            capture_output=True,
            text=True,
            timeout=30
        )
        if result.returncode == 0:
            return True, None
        else:
            # Extract first error line
            error_lines = result.stderr.split('\n')
            first_error = next((line for line in error_lines if ": error:" in line), error_lines[0] if error_lines else "")
            return False, first_error[:200]
    except subprocess.TimeoutExpired:
        return False, "Compilation timeout"
    except Exception as e:
        return False, str(e)

def process_wit_file(wit_path):
    """Process a single WIT file - generate and compile"""
    stub_name = get_stub_name(wit_path)
    rel_path = wit_path.relative_to(WIT_BINDGEN_DIR)
    
    # Generate stub
    gen_ok, gen_error = generate_stub(wit_path, stub_name)
    
    if gen_ok:
        # Check if this generated an empty stub (references external packages only)
        if check_if_empty_stub(stub_name):
            return rel_path, 'empty_stub', 'Generated empty stub (references external packages only)'
        
        # Compile stub
        compile_ok, compile_error = compile_stub(stub_name)
        
        if compile_ok:
            return rel_path, 'success', None
        else:
            return rel_path, 'compile_failed', compile_error
    else:
        return rel_path, 'gen_failed', gen_error

def main():
    print("WIT-Bindgen Test Suite Validation")
    print("=" * 60)
    
    # Ensure build directory exists
    if not WIT_CODEGEN.exists():
        print(f"{RED}Error: wit-codegen not found at {WIT_CODEGEN}{RESET}")
        print("Please build the project first")
        return 1
    
    # Create output directory
    GENERATED_DIR.mkdir(parents=True, exist_ok=True)
    
    # Find all WIT files
    wit_files = find_all_wit_files()
    num_workers = min(multiprocessing.cpu_count(), len(wit_files))
    print(f"Found {len(wit_files)} WIT files in wit-bindgen test suite")
    print(f"Using {num_workers} parallel workers\n")
    
    # Statistics
    stats = {
        'total': len(wit_files),
        'gen_success': 0,
        'gen_failed': 0,
        'compile_success': 0,
        'compile_failed': 0,
        'empty_stubs': 0
    }
    
    failed_generation = []
    failed_compilation = []
    empty_stub_files = []
    
    # Process files in parallel
    with ThreadPoolExecutor(max_workers=num_workers) as executor:
        # Submit all jobs
        futures = {executor.submit(process_wit_file, wit_path): wit_path for wit_path in wit_files}
        
        # Process results as they complete
        for idx, future in enumerate(as_completed(futures), 1):
            wit_path = futures[future]
            try:
                rel_path, result, error = future.result()
                
                if result == 'success':
                    stats['gen_success'] += 1
                    stats['compile_success'] += 1
                    status = f"{GREEN}✓{RESET}"
                elif result == 'empty_stub':
                    stats['gen_success'] += 1
                    stats['empty_stubs'] += 1
                    status = f"{YELLOW}⊘{RESET}"
                    empty_stub_files.append((rel_path, error))
                elif result == 'compile_failed':
                    stats['gen_success'] += 1
                    stats['compile_failed'] += 1
                    status = f"{RED}✗{RESET}"
                    failed_compilation.append((rel_path, error))
                elif result == 'gen_failed':
                    stats['gen_failed'] += 1
                    status = f"{YELLOW}⚠{RESET}"
                    failed_generation.append((rel_path, error))
                
                # Print progress
                print(f"[{idx:3d}/{stats['total']:3d}] {status} {str(rel_path):60s}", end='')
                if idx % 3 == 0:
                    print()
                else:
                    print("  ", end='')
                sys.stdout.flush()
                
            except Exception as e:
                rel_path = wit_path.relative_to(WIT_BINDGEN_DIR)
                print(f"[{idx:3d}/{stats['total']:3d}] {RED}✗{RESET} {str(rel_path):60s}  (exception: {e})")
    
    print("\n")
    print("=" * 60)
    print("Summary:")
    print(f"  Total WIT files:        {stats['total']}")
    print(f"  Generation successful:  {stats['gen_success']} ({stats['gen_success']/stats['total']*100:.1f}%)")
    print(f"  Generation failed:      {stats['gen_failed']}")
    print(f"  Empty stubs (no types): {stats['empty_stubs']}")
    print(f"  Compilation successful: {stats['compile_success']} ({stats['compile_success']/stats['total']*100:.1f}%)")
    print(f"  Compilation failed:     {stats['compile_failed']}")
    
    if empty_stub_files:
        print(f"\n{YELLOW}Empty stubs ({len(empty_stub_files)} files):{RESET}")
        print(f"These files only reference external packages and contain no concrete definitions:")
        for rel_path, reason in empty_stub_files[:10]:
            print(f"  - {rel_path}")
        if len(empty_stub_files) > 10:
            print(f"  ... and {len(empty_stub_files) - 10} more")
    
    if failed_generation:
        print(f"\n{YELLOW}Failed generation ({len(failed_generation)} files):{RESET}")
        for rel_path, error in failed_generation[:10]:  # Show first 10
            print(f"  - {rel_path}")
            if error:
                print(f"    {error}")
        if len(failed_generation) > 10:
            print(f"  ... and {len(failed_generation) - 10} more")
    
    if failed_compilation:
        print(f"\n{RED}Failed compilation ({len(failed_compilation)} files):{RESET}")
        for rel_path, error in failed_compilation[:10]:  # Show first 10
            print(f"  - {rel_path}")
            if error:
                print(f"    {error}")
        if len(failed_compilation) > 10:
            print(f"  ... and {len(failed_compilation) - 10} more")
    
    success_rate = stats['compile_success'] / stats['total'] * 100
    print(f"\n{GREEN if success_rate > 80 else YELLOW if success_rate > 50 else RED}Overall success rate: {success_rate:.1f}%{RESET}")
    
    return 0 if stats['compile_failed'] == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
