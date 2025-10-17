[![MacOS](https://github.com/GordonSmith/component-model-cpp/actions/workflows/macos.yml/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions/workflows/macos.yml)
[![Windows](https://github.com/GordonSmith/component-model-cpp/actions/workflows/windows.yml/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions/workflows/windows.yml)
[![Ubuntu](https://github.com/GordonSmith/component-model-cpp/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions/workflows/ubuntu.yml)
[![codecov](https://codecov.io/gh/GordonSmith/component-model-cpp/graph/badge.svg?token=CORP310T92)](https://codecov.io/gh/GordonSmith/component-model-cpp)

<p align="center">
  <img src="https://github.com/WebAssembly/WASI/blob/main/WASI.png?raw=true" height="160" width="auto" />
  <img src="https://repository-images.githubusercontent.com/254842585/4dfa7580-7ffb-11ea-99d0-46b8fe2f4170" height="160" width="auto" />
</p>

# Component Model C++ (Developer Reference)

This repository provides a header-only C++20 implementation of the WebAssembly Component Model canonical ABI. The code mirrors the official [Python Reference](https://github.com/GordonSmith/component-model-cpp/blob/main/ref/component-model/design/mvp/canonical-abi/definitions.py) and the [Specification](https://github.com/GordonSmith/component-model-cpp/blob/main/ref/component-model/design/mvp/CanonicalABI.md). All behavior (naming, state transitions, error conditions) should remain aligned with the reference.

## Official Documentation, Issues, and Discussions


> [!WARNING]
> **⚠️ 🚧 DOCUMENTATION UNDER CONSTRUCTION 🚧 ⚠️**
> 
> **The documentation is being reorganized and expanded.** Expect incomplete sections, placeholders, and frequent updates as we improve coverage and organization.

* [Official Documentation](https://GordonSmith.github.io/component-model-cpp/)
* [GitHub Issues](https://github.com/GordonSmith/component-model-cpp/issues)
* [GitHub Discussions](https://github.com/GordonSmith/component-model-cpp/discussions)

## Development Notes
_The following notes are intended for developers working on this repository. See the [online documentation](https://GordonSmith.github.io/component-model-cpp/) for usage information._

### Scope & Goals
Primary focus:
* Marshal values between C++ and wasm flat representations (`lift_flat_values`, `lower_flat_values`).
* Implement canonical resource, async, and error-context operations exactly per spec.
* Offer a lightweight cooperative async runtime (`Store`, `Thread`, `Call`, `Task`).

Non-goals: adding divergence from the spec, producing alternative memory layouts, or generating separate summary documentation (commit history + code comments are preferred).

### Repository Layout (key paths)
```
include/                -> Header-only implementation (public surface via cmcpp.hpp)
  cmcpp/*.hpp           -> Trait definitions, lift/lower logic, async/runtime, resources
  boost/*.*             -> Bundled Boost.PFR (license included) minimal reflection support (used for struct/record mappings, will be removed when c++26 is widely adopted )
  cmcpp.hpp             -> Aggregate public include (pulls in full cmcpp namespace)
  wamr.hpp              -> WAMR host integration helpers
  pfr.hpp               -> Boost.PFR lightweight reflection shim used internally
grammar/                -> ANTLR WIT grammar and Shiki syntax highlighting for docs
tools/wit-codegen/      -> Binding generation utilities (WIT -> C++ host stubs)
test/                   -> doctest suites + ICU-backed string conversion helpers
samples/                -> Optional WAMR host sample (ENABLE via BUILD_SAMPLES)
ref/                    -> Canonical spec, Python reference, test suite
vcpkg/                  -> third-party dependencies via vcpkg
vcpkg_overlays/         -> custom vcpkg overlays for WAMR, wasi-sdk, etc.
```

### Core Headers & Concepts
* `traits.hpp`: `ValTrait<T>` specializations declare `size`, `alignment`, and `flat_types`.
* `context.hpp`: `LiftLowerContext`, `ComponentInstance`, resource tables, canonical ops.
* `runtime.hpp`: Cooperative scheduling primitives (`Store`, `Thread`, `Call`, `Task`).
* `error_context.hpp`: Lifecycle for canonical error contexts.
* `load.hpp` / `store.hpp`: Memory helpers that must respect trait `size` & `alignment`.
* `wamr.hpp`: Glue for mapping WAMR host functions to component model signatures.

#### Adding a New Canonical Type
1. Create / extend a `ValTrait` specialization with correct `flat_types` sequence.
2. Ensure lift/lower templates recognize the type using existing concepts (List, Variant, Flags, etc.).
3. Add tests mirroring Python reference edge cases.
4. Cross-check `definitions.py` for identical error/trap conditions.

### Build & Dependencies
Prerequisites: CMake ≥3.22 (for cmake presets), C++20 compiler, Java Runtime (for ANTLR).
Optional: Rust (`wasm-tools`, `wit-bindgen-cli`) for regenerating sample wasm artifacts.

#### Preset Build (recommended)
Linux:
```bash
cmake --preset linux-ninja-Debug
cmake --build --preset linux-ninja-Debug
ctest --test-dir build --output-on-failure
```
Windows Visual Studio 17 (2022):
```bash
cmake --preset vcpkg-VS-17
cmake --build --preset VS-17-Debug
ctest --test-dir build -C Debug --output-on-failure
```
Header-only packaging (no tests/samples/wit-codegen):
```bash
cmake -DBUILD_TESTING=OFF -DBUILD_SAMPLES=OFF -DWIT_CODEGEN=OFF -S . -B build
cmake --build build
```

#### Default Options
- `BUILD_TESTING` (ON)
- `BUILD_SAMPLES` (ON)
- `WIT_CODEGEN` (ON)

#### Preset Build (recommended)

Linux:
```bash
cmake --preset linux-ninja-Release
cmake --build --preset linux-ninja-Release
ctest --test-dir build --output-on-failure
```

Windows Visual Studio 17 (2022):
```bash
cmake --preset vcpkg-VS-17
cmake --build --preset VS-17-Release
ctest --test-dir build -C Release --output-on-failure
```

#### Manual Build

Clone this repository and from within the cloned folder, run the following commands:
```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
ctest --test-dir build --output-on-failure
```

#### Installation & Consumption

```bash
cmake --preset linux-ninja-Release
cmake --build build --target install
```

Then use:
```cmake
find_package(cmcpp REQUIRED)
target_link_libraries(my_target PRIVATE cmcpp::cmcpp)
```

CPack can emit TGZ/ZIP/DEB/RPM archives from the configured build directory. See `docs/PACKAGING.md`.

### Testing & Coverage
* C++ tests (doctest) in `test/`. Invoke via `ctest --test-dir build`.
* Coverage (Linux GCC/Clang):
```bash
lcov --capture --directory build --output-file coverage.info
lcov --remove coverage.info '/usr/include/*' '*/vcpkg/*' '*/test/*' -o coverage.filtered.info
genhtml coverage.filtered.info --output-directory coverage-html  # optional
```

### Reference Links
* Canonical Spec: `ref/component-model/design/mvp/CanonicalABI.md`
* Python Reference: `ref/component-model/design/mvp/canonical-abi/definitions.py`
* Python Test Suite: `ref/component-model/design/mvp/canonical-abi/run_tests.py`
* WAMR Integration: `include/wamr.hpp` + `samples/wamr/`
* Grammar / WIT Parsing: `grammar/` (ANTLR) → `generate-grammar` target

## Star History

<a href="https://star-history.com/#GordonSmith/component-model-cpp&Date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=GordonSmith/component-model-cpp&type=Date&theme=dark" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=GordonSmith/component-model-cpp&type=Date" />
    <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=GordonSmith/component-model-cpp&type=Date" />
  </picture>
</a>
