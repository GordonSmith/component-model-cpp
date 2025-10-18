## Repository focus
- Header-only C++20 implementation of the WebAssembly Component Model canonical ABI; public surface aggregates through `include/cmcpp.hpp` and the `cmcpp` namespace.
- Core work revolves around marshaling values between C++ types and wasm flat values using `lift_flat`, `lower_flat`, `lift_flat_values`, and `lower_flat_values` templates.
- **Implementation mirrors the official Python reference**: The C++ code structure, type names, and semantics directly correspond to the Python implementation in `ref/component-model/design/mvp/canonical-abi/definitions.py` and the specification in `ref/component-model/design/mvp/CanonicalABI.md`. When making changes, cross-reference these files to ensure alignment.
- Supports async runtime primitives (`Store`, `Thread`, `Task`), resource management (`own`, `borrow`), streams/futures, waitables, and error contexts.

## Working style
- **Do not create summary documents**: Avoid creating markdown files in `docs/` to summarize work sessions, changes, or fixes unless explicitly requested. The git commit history and code comments are sufficient documentation.
- Focus on making changes directly to code, updating existing documentation only when functionality changes, and providing concise summaries in responses rather than generating files.

## Core headers
- `include/cmcpp/traits.hpp` defines `ValTrait`, type aliases (e.g., `bool_t`, `string_t`), and concept shortcuts that every new type must implement.
- `include/cmcpp/context.hpp` encapsulates `LiftLowerContext`, `ComponentInstance`, `Task`, resource tables (`HandleTable`, `InstanceTable`), and canonical operations like `canon_waitable_*`, `canon_stream_*`, `canon_future_*`.
- `include/cmcpp/runtime.hpp` provides async primitives: `Store`, `Thread`, `Call`, and supporting types for cooperative scheduling.
- `include/cmcpp/error_context.hpp` implements error context lifecycle (`canon_error_context_new/debug_message/drop`).
- Memory helpers live in `include/cmcpp/load.hpp`/`store.hpp`; always respect `ValTrait<T>::size` and `alignment` from those headers instead of hard-coding layout rules.

## Lift/Lower workflow
- `lower_flat_values` flattens function arguments, auto-switching to heap marshalling when the arity exceeds `MAX_FLAT_PARAMS`; `lift_flat_values` mirrors this when decoding results.
- Heap-based lowering uses `cx.opts.realloc` to allocate guest memory; make sure any caller populates `LiftLowerOptions` with a working realloc when passing heap-backed types (lists, strings, large tuples).
- String conversions rely on the `convert` callback; tests wire this through ICU (`test/host-util.cpp`). Preserve the encoding enum contract (`Encoding::Utf8`, `Encoding::Utf16`, `Encoding::Latin1`, `Encoding::Latin1_Utf16`).

## Adding types or functions
- Extend `ValTrait` specializations and `ValTrait<T>::flat_types` for new composite types before touching lift/lower logic.
- Use concepts from `traits.hpp` (e.g., `List`, `Variant`, `Flags`) so overload resolution stays consistent; mirror patterns in `include/cmcpp/list.hpp`, `variant.hpp`, etc.
- Guard traps with `trap_if(cx, condition, message)` and route all host failures through the `HostTrap` provided by the context rather than throwing ad-hoc exceptions.
- **Maintain Python equivalence**: When implementing new canonical operations or type handling, ensure the C++ behavior matches the corresponding Python code in `definitions.py`. Type names, state transitions, and error conditions should align with the reference implementation.

## Testing
- Default build enables coverage flags on GCC/Clang; run `cmake --preset linux-ninja-Debug` followed by `cmake --build --preset linux-ninja-Debug` and then `ctest --output-on-failure` from the build tree. When launching tests from the repository root, add `--test-dir build` (for example `ctest --test-dir build --output-on-failure`) because calling `ctest` without a build directory just reports "No tests were found!!!".
- C++ tests live in `test/main.cpp` using doctest and ICU for encoding checks; keep new tests near related sections for quick discovery.
- Test coverage workflow: run tests, then `lcov --capture --directory . --output-file coverage.info`, filter with `lcov --remove`, and optionally generate HTML with `genhtml`. See README for full workflow.
- **Python reference tests**: The canonical ABI test suite in `ref/component-model/design/mvp/canonical-abi/run_tests.py` exercises the same ABI rules—use it to cross-check tricky canonical ABI edge cases when changing flattening behavior. Run with `python3 run_tests.py` (requires Python 3.10+).
- When adding new features to the C++ implementation, verify behavior matches the Python reference by comparing against `definitions.py` logic and running the Python test suite.

## Samples & runtimes
- Enabling `-DBUILD_SAMPLES=ON` builds the WAMR host sample (`samples/wamr`); it wraps host callbacks with `host_function`/`guest_function` helpers defined in `include/wamr.hpp`.
- Sample guest wasm artifacts are generated from `wasm/test.wit` via `wit-bindgen` and `wasm-tools`. The `samples/CMakeLists.txt` external project expects `WASI_SDK_PREFIX` to point at the vcpkg-installed WASI SDK.
- `wasm/build.sh` spins up a dev container to compile wasm components reproducibly; run it if you need to refresh the sample binaries.

## Tooling notes
- Dependencies are managed through `vcpkg.json` with overlays in `vcpkg_overlays/` (notably for WAMR); stick with preset builds so CMake wires in the correct toolchain file automatically.
- Cargo manifest (`Cargo.toml`) is only for fetching `wasm-tools` and `wit-bindgen-cli`; if you touch wasm generation logic, update both the manifest and any scripts referencing those versions.
- Keep documentation alongside code: update `README.md` when introducing new host types or workflows so downstream integrators stay aligned with the canonical ABI behavior.
- When you need a patch file, prefer generating it with git: stage the changes you want included, run `git diff --staged > <name>.patch`, and double-check the file starts with `---`/`+++` headers before using it in `PATCHES` lists.
