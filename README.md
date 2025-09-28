[![Actions Status](https://github.com/GordonSmith/component-model-cpp/workflows/MacOS/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions)
[![Actions Status](https://github.com/GordonSmith/component-model-cpp/workflows/Windows/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions)
[![Actions Status](https://github.com/GordonSmith/component-model-cpp/workflows/Ubuntu/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions)
[![codecov](https://codecov.io/gh/GordonSmith/component-model-cpp/graph/badge.svg?token=CORP310T92)](https://codecov.io/gh/GordonSmith/component-model-cpp)

<p align="center">
  <img src="https://github.com/WebAssembly/WASI/blob/main/WASI.png?raw=true" height="175" width="auto" />
  <img src="https://repository-images.githubusercontent.com/254842585/4dfa7580-7ffb-11ea-99d0-46b8fe2f4170" height="175" width="auto" />
</p>

# Component Model C++

This repository contains a C++ ABI implementation of the WebAssembly Component Model.

## Features

### OS
- [x] Ubuntu 24.04
- [x] MacOS 13
- [x] MacOS 14 (Arm)
- [x] Windows 2019
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
- [x] String
- [x] utf8 String
- [x] utf16 String
- [x] latin1+utf16 String
- [x] List
- [x] Record
- [x] Tuple
- [x] Variant
- [x] Enum
- [x] Option
- [x] Result
- [x] Flags
- [ ] Own
- [ ] Borrow

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

- `-DBUILD_TESTING=ON/OFF` - Enable/disable building tests (requires doctest, ICU)
- `-DBUILD_SAMPLES=ON/OFF` - Enable/disable building samples (requires wasi-sdk)
- `-DCMAKE_BUILD_TYPE=Debug/Release/RelWithDebInfo/MinSizeRel` - Build configuration

## Usage

This library is a header only library. To use it in your project, you can:
- [x] Copy the contents of the `include` directory to your project.
- [ ] Use `vcpkg` to install the library and its dependencies.

### Async runtime helpers

The canonical Component Model runtime is cooperative: hosts must drive pending work by scheduling tasks explicitly. `cmcpp` now provides a minimal async harness in `cmcpp/runtime.hpp`:

- `Store` owns the pending task queue and exposes `invoke` plus `tick()`.
- `FuncInst` is the callable signature hosts use to wrap guest functions.
- `Thread::create` builds a pending task with user-supplied readiness/resume callbacks.
- `Call::from_thread` returns a cancellation-capable handle to the caller.

Typical usage:

```cpp
cmcpp::Store store;
cmcpp::FuncInst func = [](cmcpp::Store &store,
              cmcpp::SupertaskPtr,
              cmcpp::OnStart on_start,
              cmcpp::OnResolve on_resolve) {
  auto args = std::make_shared<std::vector<std::any>>(on_start());
  auto gate = std::make_shared<std::atomic<bool>>(false);

  auto thread = cmcpp::Thread::create(
    store,
    [gate]() { return gate->load(); },
    [args, on_resolve](bool cancelled) {
      on_resolve(cancelled ? std::nullopt : std::optional{*args});
      return false; // finished
    },
    true,
    [gate]() { gate->store(true); });

  return cmcpp::Call::from_thread(thread);
};

auto call = store.invoke(func, nullptr, [] { return std::vector<std::any>{}; }, [](auto) {});
// Drive progress
store.tick();
```

### Waitables, streams, and futures

The canonical async ABI surfaces are implemented via `canon_waitable_*`, `canon_stream_*`, and `canon_future_*` helpers on `ComponentInstance`. Waitable sets can be joined to readable/writable stream ends or futures, and `canon_waitable_set_poll` reports readiness using the same event payload layout defined by the spec. See the doctests in `test/main.cpp` for end-to-end examples.

Call `tick()` in your host loop until all pending work completes. Cancellation is cooperative: calling `Call::request_cancellation()` marks the associated thread as cancelled before the next `tick()`.

 
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
