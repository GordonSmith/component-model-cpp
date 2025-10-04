[![MacOS](https://github.com/GordonSmith/component-model-cpp/actions/workflows/macos.yml/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions/workflows/macos.yml)
[![Windows](https://github.com/GordonSmith/component-model-cpp/actions/workflows/windows.yml/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions/workflows/windows.yml)
[![Ubuntu](https://github.com/GordonSmith/component-model-cpp/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions/workflows/ubuntu.yml)
[![codecov](https://codecov.io/gh/GordonSmith/component-model-cpp/graph/badge.svg?token=CORP310T92)](https://codecov.io/gh/GordonSmith/component-model-cpp)

<p align="center">
  <img src="https://github.com/WebAssembly/WASI/blob/main/WASI.png?raw=true" height="175" width="auto" />
  <img src="https://repository-images.githubusercontent.com/254842585/4dfa7580-7ffb-11ea-99d0-46b8fe2f4170" height="175" width="auto" />
</p>

# Component Model C++

This repository contains a C++ ABI implementation of the WebAssembly Component Model.

## Release management

Automated releases are handled by [Release Please](https://github.com/googleapis/release-please) via GitHub Actions. Conventional commit messages (`feat:`, `fix:`, etc.) keep the changelog accurate and drive version bumps; when enough changes accumulate, the workflow opens a release PR that can be merged to publish a GitHub release.

See [docs/RELEASE.md](docs/RELEASE.md) for complete documentation on the automated release process, including how to create releases and what packages are generated.

## Features

### OS
- [x] Ubuntu 24.04
- [ ] MacOS 13
- [ ] MacOS 14 (Arm)
- [ ] Windows 2019
- [x] Windows 2022

### Host Data Types
- [x] Bool
- [x] S8
- [x] U8
- [x] S16
- [x] U16
- [x] S32
- [x] U32
- [x] S64
- [x] U64
- [x] F32
- [x] F64
- [x] Char
- [x] Strings (UTF-8, UTF-16, Latin-1+UTF-16)
- [x] List
- [x] Record
- [x] Tuple
- [x] Variant
- [x] Enum
- [x] Option
- [x] Result
- [x] Flags
- [x] Streams (readable/writable)
- [x] Futures (readable/writable)
- [x] Own
- [x] Borrow

### Host Functions
- [x] lower_flat_values
- [x] lift_flat_values

### Tests / Samples
- [x] ABI
- [ ] WasmTime
- [x] Wamr
- [ ] WasmEdge

When expanding the canonical ABI surface, cross-check the Python reference tests in `ref/component-model/design/mvp/canonical-abi/run_tests.py`; new host features should mirror the behaviors exercised there.

## Build Instructions

### Prerequisites

- **CMake** 3.5 or higher (3.22+ recommended for presets)
- **C++20 compatible compiler**
- **vcpkg** for dependency management
- **Rust toolchain** with `cargo` (for additional tools)

#### Platform-specific requirements

**Ubuntu/Linux:**
```bash
sudo apt-get install -y autoconf autoconf-archive automake build-essential ninja-build

# Optional: for creating RPM packages
sudo apt-get install -y rpm
```

**macOS:**
```bash
brew install pkg-config autoconf autoconf-archive automake coreutils libtool cmake ninja
```

**Windows:**
- Visual Studio 2019 or 2022 with C++ support

#### Rust tools (required for samples and tests)
```bash
cargo install wasm-tools wit-bindgen-cli
```

### Basic Build (Header-only)

For header-only usage without tests or samples:

```bash
git clone https://github.com/LexisNexis-GHCPE/component-model-cpp.git
cd component-model-cpp
git submodule update --init --recursive

mkdir build && cd build
cmake .. -DBUILD_TESTING=OFF -DBUILD_SAMPLES=OFF
cmake --build .
```

### Build with Dependencies (Tests & Samples)

Using CMake presets with vcpkg:

#### Linux
```bash
git clone https://github.com/LexisNexis-GHCPE/component-model-cpp.git
cd component-model-cpp
git submodule update --init --recursive

# Configure and build
cmake --preset linux-ninja-Debug
cmake --build --preset linux-ninja-Debug

# Run tests
cd build && ctest -VV
```

#### Windows
```bash
git clone https://github.com/LexisNexis-GHCPE/component-model-cpp.git
cd component-model-cpp
git submodule update --init --recursive

# Configure and build
cmake --preset vcpkg-VS-17
cmake --build --preset VS-17-Debug

# Run tests
cd build && ctest -C Debug -VV
```

#### macOS
```bash
git clone https://github.com/LexisNexis-GHCPE/component-model-cpp.git
cd component-model-cpp
git submodule update --init --recursive

# Configure and build
cmake --preset linux-ninja-Debug
cmake --build --preset linux-ninja-Debug

# Run tests
cd build && ctest -VV
```

### Manual Build without Presets

If you prefer not to use CMake presets:

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
ctest -VV  # Run tests
```

### Build Options

The following CMake options control what gets built:

- `BUILD_TESTING` (default: ON) - Build unit tests
- `BUILD_SAMPLES` (default: ON) - Build sample applications demonstrating runtime integration
- `BUILD_GRAMMAR` (default: OFF) - Generate C++ code from ANTLR grammar for WIT parsing

Example:
```bash
cmake -DBUILD_TESTING=ON -DBUILD_SAMPLES=ON -DBUILD_GRAMMAR=ON ..
```

#### Grammar Code Generation

The project includes an ANTLR grammar for parsing WebAssembly Interface Types (WIT). To generate C++ parser code:

```bash
# Enable grammar generation during configuration
cmake -DBUILD_GRAMMAR=ON ..

# Generate the code
cmake --build . --target generate-grammar
```

**Requirements:**
- Java runtime (for ANTLR)
- The ANTLR jar is automatically downloaded during CMake configuration

Generated C++ files are compiled into a static library `wit-grammar` in the build tree that can be linked by tools. See [grammar/README.md](grammar/README.md) for details.


### Coverage

The presets build tests with GCC/Clang coverage instrumentation enabled, so generating a report is mostly a matter of running the suite and capturing the counters. On Ubuntu the full workflow looks like:

```bash
# 1. Install tooling (once per machine)
sudo apt-get update
sudo apt-get install -y lcov

# 2. Rebuild and rerun tests to refresh .gcda files
cmake --build --preset linux-ninja-Debug
cd build
ctest --output-on-failure

# 3. Capture raw coverage data
lcov --capture --directory . --output-file coverage.info

# 4. Filter out system headers, vcpkg packages, and tests (optional but recommended)
lcov --remove coverage.info '/usr/include/*' '*/vcpkg/*' '*/test/*' \
  --output-file coverage.filtered.info --ignore-errors unused

# 5. Inspect the summary or render HTML
lcov --list coverage.filtered.info
genhtml coverage.filtered.info --output-directory coverage-html  # optional
```

Generated artifacts live in the `build/` directory (`coverage.info`, `coverage.filtered.info`, and optionally `coverage-html/`). The same commands work on other platforms once the equivalent of `lcov` (or LLVM's `llvm-cov`) is installed.

## Installation and Packaging

### Installing Locally

Install cmcpp to a local directory (default: `build/stage`):

```bash
cmake --preset linux-ninja-Debug
cmake --build build
cmake --build build --target install
```

### Using in Other CMake Projects

Once installed, use `find_package()` to integrate cmcpp:

```cmake
find_package(cmcpp REQUIRED)
target_link_libraries(my_app PRIVATE cmcpp::cmcpp)
```

Build your project:

```bash
cmake . -DCMAKE_PREFIX_PATH=/path/to/cmcpp/install
cmake --build .
```

### Creating Distribution Packages

Generate packages with CPack:

```bash
cd build

# All default packages for your platform
cpack

# Specific formats
cpack -G TGZ          # Tar.gz archive (cross-platform)
cpack -G DEB          # Debian package (.deb)
cpack -G RPM          # RPM package (.rpm) - requires 'rpm' package installed
cpack -G ZIP          # ZIP archive (Windows)
```

**Note:** To create RPM packages on Ubuntu/Debian, install the `rpm` package first:
```bash
sudo apt-get install -y rpm
```

Packages include:
- Complete header-only library
- CMake config files for `find_package()`
- `wit-codegen` tool for generating C++ bindings from WIT files (if `BUILD_GRAMMAR=ON`)

See [docs/PACKAGING.md](docs/PACKAGING.md) for complete packaging documentation.

## Usage

This library is a header only library. To use it in your project, you can:
- [x] Copy the contents of the `include` directory to your project.
- [x] Install via `cmake --build build --target install` and use `find_package(cmcpp)`.
- [ ] Use `vcpkg` to install the library and its dependencies (planned).

### Configuring `InstanceContext` and canonical options

Most host interactions begin by materialising an `InstanceContext`. This container wires together the host trap callback, string conversion routine, and the guest `realloc` export. Use `createInstanceContext` to capture those dependencies once:

```cpp
cmcpp::HostTrap trap = [](const char *msg) {
  throw std::runtime_error(msg ? msg : "trap");
};
cmcpp::HostUnicodeConversion convert = {}; // see test/host-util.cpp for an ICU-backed example
cmcpp::GuestRealloc realloc = [&](int ptr, int old_size, int align, int new_size) {
  return guest_realloc(ptr, old_size, align, new_size);
};

auto icx = cmcpp::createInstanceContext(trap, convert, realloc);
```

When preparing to lift or lower values, create a `LiftLowerContext` from the instance. Pass the guest memory span and any canonical options you need:

```cpp
cmcpp::Heap heap(4096);
cmcpp::CanonicalOptions options;
options.memory = cmcpp::GuestMemory(heap.memory.data(), heap.memory.size());
options.string_encoding = cmcpp::Encoding::Utf8;
options.realloc = icx->realloc;
options.post_return = [] { /* guest cleanup */ };
options.callback = [](cmcpp::EventCode code, uint32_t index, uint32_t payload) {
  std::printf("async event %u for handle %u (0x%x)\n",
              static_cast<unsigned>(code), index, payload);
};
options.sync = false; // allow async continuations

auto cx = icx->createLiftLowerContext(std::move(options));
cx->inst = &component_instance;
```

The canonical options determine whether async continuations are allowed (`sync`), which hook to run after a successful lowering (`post_return`), and how async notifications surface back to the embedder (`callback`). Every guest call that moves data across the ABI should use the same context until `LiftLowerContext::exit_call()` is invoked.

### Driving async flows with the runtime harness

The Component Model runtime is cooperative: hosts advance work by draining a pending queue. `cmcpp/runtime.hpp` provides the same primitives as the canonical Python reference:

- `Store` owns the queue of `Thread` objects and exposes `invoke` plus `tick()`.
- `FuncInst` is the callable signature hosts use to wrap guest functions.
- `Thread::create` builds resumable work with readiness and resume callbacks.
- `Call::from_thread` returns a handle that supports cancellation and completion queries.
- `Task` bridges canonical backpressure (`canon_task.{return,cancel}`) and ensures `ComponentInstance::may_leave` rules are enforced.

A minimal async call looks like this:

```cpp
cmcpp::Store store;

cmcpp::FuncInst guest = [](cmcpp::Store &store,
                            cmcpp::SupertaskPtr,
                            cmcpp::OnStart on_start,
                            cmcpp::OnResolve on_resolve) {
  auto args = std::make_shared<std::vector<std::any>>(on_start());
  auto ready = std::make_shared<std::atomic<bool>>(false);

  auto thread = cmcpp::Thread::create(
      store,
      [ready] { return ready->load(); },
      [args, on_resolve](bool cancelled) {
        on_resolve(cancelled ? std::nullopt : std::optional{*args});
        return false; // one-shot
      },
      /*notify_on_cancel=*/true,
      [ready] { ready->store(true); });

  return cmcpp::Call::from_thread(thread);
};

auto call = store.invoke(
    guest,
    nullptr,
    [] { return std::vector<std::any>{int32_t{7}}; },
    [](std::optional<std::vector<std::any>> values) {
      if (!values) { std::puts("cancelled"); return; }
      std::printf("resolved with %d\n", std::any_cast<int32_t>((*values)[0]));
    });

while (!call.completed()) {
  store.tick();
}
```

`Call::request_cancellation()` cooperatively aborts work before the next `tick()`, mirroring the canonical `cancel` semantics.

### Waitables, streams, futures, and other resources

`ComponentInstance` manages resource tables that back the canonical `canon_waitable_*`, `canon_stream_*`, and `canon_future_*` entry points. Hosts typically:

1. Instantiate a descriptor (`make_stream_descriptor<T>()`, `make_future_descriptor<T>()`, etc.).
2. Create handles via `canon_stream_new`/`canon_future_new`, which return packed readable/writable indices.
3. Join readable ends to a waitable set with `canon_waitable_join`.
4. Poll readiness using `canon_waitable_set_poll`, decoding the `EventCode` and payload stored in guest memory.
5. Drop resources with the corresponding `canon_*_drop_*` helpers once the guest is finished.

Streams and futures honour the canonical copy result payload layout, so the values copied into guest memory exactly match the spec. Cancellation helpers (`canon_stream_cancel_*`, `canon_future_cancel_*`) post events when the embedder requests termination, and the async callback registered in `CanonicalOptions` receives the same event triplet that the waitable set reports.

For a complete walkthrough, see the doctest suites in `test/main.cpp`:

- "Async runtime schedules threads" demonstrates `Store`, `Thread`, `Call`, and cancellation.
- "Waitable set surfaces stream readiness" polls a waitable set tied to a stream.
- "Future lifecycle completes" verifies readable/writable futures.
- "Task yield, cancel, and return" exercises backpressure and async task APIs.

Those tests are ICU-enabled and run automatically via `ctest`.

 
## Related projects

- [**Component Model design and specification**](https://github.com/WebAssembly/component-model): Official Component Model specification.
- [**wit-bindgen c++ host**](https://github.com/cpetig/wit-bindgen):  C++ host support for the WebAssembly Interface Types (WIT) Bindgen tool.

## Star History

<a href="https://star-history.com/#GordonSmith/component-model-cpp&Date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=GordonSmith/component-model-cpp&type=Date&theme=dark" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=GordonSmith/component-model-cpp&type=Date" />
    <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=GordonSmith/component-model-cpp&type=Date" />
  </picture>
</a>
