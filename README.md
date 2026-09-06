[![MacOS](https://github.com/GordonSmith/component-model-cpp/actions/workflows/macos.yml/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions/workflows/macos.yml)
[![Windows](https://github.com/GordonSmith/component-model-cpp/actions/workflows/windows.yml/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions/workflows/windows.yml)
[![Ubuntu](https://github.com/GordonSmith/component-model-cpp/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions/workflows/ubuntu.yml)
[![codecov](https://codecov.io/gh/GordonSmith/component-model-cpp/graph/badge.svg?token=YFGH3VYQEA)](https://codecov.io/gh/GordonSmith/component-model-cpp)

<p align="center">
  <img src="https://github.com/WebAssembly/WASI/blob/main/WASI.png?raw=true" height="175" width="auto" />
  <img src="https://repository-images.githubusercontent.com/254842585/4dfa7580-7ffb-11ea-99d0-46b8fe2f4170" height="175" width="auto" />
</p>

# Component Model C++

This repository contains a header-only C++20 implementation of the WebAssembly
Component Model Canonical ABI. The public API is aggregated by
`include/cmcpp.hpp`; tests, code-generation tools, and runtime samples are built
as separate executables around the library.

The library uses templates, concepts, `constexpr` metadata, and `static_assert`
checks to describe Component Model values at compile time. For each supported
host type, its Component Model kind, memory size, alignment, and WebAssembly
flat representation are available through `ValTrait<T>`. This lets invalid or
unsupported type combinations fail during compilation, while lifting, lowering,
and guest-memory access remain runtime operations.

## Features

### OS
- [x] Ubuntu 24.04
- [x] Ubuntu 26.04
- [x] macOS (latest, builds and passes tests)
- [ ] Windows 2019
- [x] Windows 2022

### Host Data Types
The C++ aliases use the canonical value kind followed by `_t`: for example,
canonical `OptionType(T)` is represented as `cmcpp::option_t<T>`, and
`ListType(T)` as `cmcpp::list_t<T>`.

| Type | Real-world example |
| --- | --- |
| Bool | `cmcpp::bool_t feature_enabled` |
| S8 | `int8_t temperature_delta` |
| U8 | `uint8_t channel_id` |
| S16 | `int16_t altitude_delta` |
| U16 | `uint16_t network_port` |
| S32 | `int32_t file_offset` |
| U32 | `uint32_t message_length` |
| S64 | `int64_t timestamp` |
| U64 | `uint64_t byte_count` |
| F32 | `cmcpp::float32_t sensor_reading` |
| F64 | `cmcpp::float64_t exchange_rate` |
| Char | `cmcpp::char_t initial` |
| Strings (UTF-8, UTF-16, Latin-1+UTF-16) | `cmcpp::string_t user_name` |
| List | `cmcpp::list_t<cmcpp::string_t> tags` |
| Fixed-length list | `cmcpp::fixed_list_t<cmcpp::float32_t, 3> rgb` |
| Map | `cmcpp::map_t<cmcpp::string_t, uint32_t> inventory` |
| Record | `cmcpp::record_t<Account> account` |
| Tuple | `cmcpp::tuple_t<cmcpp::string_t, uint32_t> user_id_and_age` |
| Variant | `cmcpp::variant_t<cmcpp::string_t, int32_t> setting` |
| Enum | `cmcpp::enum_t<OrderStatus> status` |
| Option | `cmcpp::option_t<cmcpp::string_t> middle_name` |
| Result | `cmcpp::result_t<Order, cmcpp::string_t> response` |
| Flags | `cmcpp::flags_t<"read", "write", "admin"> permissions` |
| Streams (readable/writable) | `cmcpp::make_stream_descriptor<LogEntry>()` |
| Futures (readable/writable) | `cmcpp::make_future_descriptor<Response>()` |
| Own | An owned `ResourceType` handle for a file or socket |
| Borrow | A borrowed `ResourceType` handle passed to a call |

### Host Data Type Examples

**Bool.** Use `cmcpp::bool_t` for a two-state setting such as whether a feature is enabled. A host can lower `feature_enabled` directly and let the canonical ABI represent it as an `i32` value.

```cpp
cmcpp::bool_t feature_enabled = true;
```

**S8.** A signed 8-bit value is useful for a small signed delta, such as `int8_t temperature_delta` in a thermostat message. Values stay compact while preserving negative changes.

```cpp
int8_t temperature_delta = -2;
```

**U8.** Use `uint8_t channel_id` for a small non-negative identifier such as a radio channel, protocol version, or palette index.

```cpp
uint8_t channel_id = 11;
```

**S16.** A signed 16-bit value fits measurements such as `int16_t altitude_delta`, where the value may represent a climb or descent relative to a reference point.

```cpp
int16_t altitude_delta = -120;
```

**U16.** Use `uint16_t network_port` for a TCP or UDP port number. The unsigned range also works well for bounded counters and protocol fields.

```cpp
uint16_t network_port = 443;
```

**S32.** File offsets and signed coordinate deltas are common `int32_t` values. For example, `int32_t file_offset` can represent a position relative to the beginning of a mapped region.

```cpp
int32_t file_offset = 4096;
```

**U32.** Use `uint32_t message_length` for a byte length, record count, or other non-negative value whose range is larger than 16 bits.

```cpp
uint32_t message_length = 1024;
```

**S64.** Timestamps represented as signed 64-bit values can carry time values or differences across a wide range. A host might exchange `int64_t timestamp` in microseconds from an agreed epoch.

```cpp
int64_t timestamp = 1'725'000'000'000'000;
```

**U64.** Use `uint64_t byte_count` for large file sizes, monotonically increasing sequence numbers, or counters that must not become negative.

```cpp
uint64_t byte_count = 12'000'000'000ULL;
```

**F32.** `cmcpp::float32_t sensor_reading` is appropriate when a sensor or graphics pipeline prioritizes compact storage and single-precision range.

```cpp
cmcpp::float32_t sensor_reading = 21.5f;
```

**F64.** Use `cmcpp::float64_t exchange_rate` for calculations where accumulated rounding error matters, such as currency conversion or geographic coordinates.

```cpp
cmcpp::float64_t exchange_rate = 1.0842;
```

**Char.** `cmcpp::char_t initial` stores a Unicode scalar value, making it suitable for a user initial, a parsed code point, or a single internationalized label character.

```cpp
cmcpp::char_t initial = U'G';
```

**Strings.** Use `cmcpp::string_t user_name` for UTF-8 text, `cmcpp::u16string_t` for UTF-16-oriented APIs, or `cmcpp::latin1_u16string_t` when the canonical encoding may be Latin-1 or UTF-16.

```cpp
cmcpp::string_t user_name = "Grace";
cmcpp::u16string_t display_name = u"Grace";
```

**List.** A `cmcpp::list_t<cmcpp::string_t> tags` models a variable-length collection such as search labels or capabilities. The list representation carries both its guest-memory pointer and its element count.

```cpp
cmcpp::list_t<cmcpp::string_t> tags = {"wasm", "cpp"};
```

**Fixed-length list.** `cmcpp::fixed_list_t<cmcpp::float32_t, 3> rgb` models exactly three color channels. The length is part of the C++ type, so a four-channel value cannot be passed accidentally where RGB is required.

```cpp
cmcpp::fixed_list_t<cmcpp::float32_t, 3> rgb = {0.2f, 0.4f, 0.8f};
```

**Map.** Use `cmcpp::map_t<cmcpp::string_t, uint32_t> inventory` for keyed data such as item names and quantities. The canonical representation treats the map as a list of key-value tuples.

```cpp
cmcpp::map_t<cmcpp::string_t, uint32_t> inventory{{"books", 4}};
```

**Record.** A user-defined aggregate such as `struct Account { uint32_t id; cmcpp::string_t email; };` can be wrapped as `cmcpp::record_t<Account>`. Its fields are lowered in declaration order with canonical alignment.

```cpp
struct Account {
  uint32_t id;
  cmcpp::string_t email;
};

cmcpp::record_t<Account> account{7, "user@example.com"};
auto flat_account = cmcpp::lower_flat(cx, account);
```

**Tuple.** `cmcpp::tuple_t<cmcpp::string_t, uint32_t> user_id_and_age` is useful for a small unnamed pair returned by a helper. Use a record instead when the fields need stable, readable names.

```cpp
using UserIdAndAge = cmcpp::tuple_t<cmcpp::string_t, uint32_t>;
UserIdAndAge user_id_and_age{"user-7", 42};
auto user_id = std::get<0>(user_id_and_age);
auto age = std::get<1>(user_id_and_age);
```

**Variant.** `cmcpp::variant_t<cmcpp::string_t, int32_t> setting` can represent a setting supplied either as text or as a numeric value. The active alternative becomes the canonical discriminant and payload.

```cpp
using Setting = cmcpp::variant_t<cmcpp::string_t, int32_t>;
Setting text_setting{cmcpp::string_t{"dark"}};
Setting numeric_setting{30};
auto flat_setting = cmcpp::lower_flat(cx, text_setting);
```

**Enum.** `cmcpp::enum_t<OrderStatus> status` represents a closed set such as pending, shipped, and cancelled. The WIT enum supplies the labels while the C++ representation carries its numeric discriminant.

```cpp
enum class OrderStatus : uint32_t { pending, shipped, cancelled };
cmcpp::enum_t<OrderStatus> status = static_cast<uint32_t>(OrderStatus::shipped);
bool is_complete = status == static_cast<uint32_t>(OrderStatus::cancelled);
```

**Option.** `cmcpp::option_t<cmcpp::string_t> middle_name` distinguishes an absent middle name from an empty string. This corresponds to the canonical `none` and `some` cases rather than using a sentinel string.

```cpp
using MiddleName = cmcpp::option_t<cmcpp::string_t>;
MiddleName present = cmcpp::string_t{"Ada"};
MiddleName absent = std::nullopt;
if (present) {
  auto name = *present;
}
```

**Result.** `cmcpp::result_t<Order, cmcpp::string_t> response` models an operation that either returns an order or an error message. The success and error alternatives remain distinct even though both travel through the same canonical result shape.

```cpp
struct Order {
  uint32_t id;
  cmcpp::string_t status;
};
using OrderResult = cmcpp::result_t<cmcpp::record_t<Order>, cmcpp::string_t>;
OrderResult success{cmcpp::record_t<Order>{42, "shipped"}};
OrderResult failure{cmcpp::string_t{"order not found"}};
```

**Flags.** `cmcpp::flags_t<"read", "write", "admin"> permissions` describes independent capabilities. Individual labels can be tested or changed with the flag helpers instead of manually managing a bit mask.

```cpp
cmcpp::flags_t<"read", "write", "admin"> permissions;
permissions.set<"read">();
permissions.set<"write">();
if (permissions.test<"read">() && !permissions.test<"admin">()) {
  // Read and write are allowed; administration is not.
}
```

**Streams.** `cmcpp::make_stream_descriptor<LogEntry>()` describes a stream of log entries. Readable and writable ends can be created with the canonical stream operations, joined to a waitable set, and copied incrementally through guest memory.

```cpp
auto log_stream = cmcpp::make_stream_descriptor<LogEntry>();
uint64_t ends = cmcpp::canon_stream_new(instance, log_stream, trap);
uint32_t readable = static_cast<uint32_t>(ends);
uint32_t writable = static_cast<uint32_t>(ends >> 32);
cmcpp::canon_stream_drop_readable(instance, readable, trap);
cmcpp::canon_stream_drop_writable(instance, writable, trap);
```

**Futures.** `cmcpp::make_future_descriptor<Response>()` describes one eventual response. A readable end can wait for one value while a writable end completes it, with cancellation and readiness reported through the canonical future operations.

```cpp
auto response_future = cmcpp::make_future_descriptor<Response>();
uint64_t ends = cmcpp::canon_future_new(instance, response_future, trap);
uint32_t readable = static_cast<uint32_t>(ends);
uint32_t writable = static_cast<uint32_t>(ends >> 32);
cmcpp::canon_future_drop_readable(instance, readable, trap);
// Complete it with canon_future_write(instance, response_future, writable, ...).
// Then drop the writable end with canon_future_drop_writable(...).
```

**Own.** An owned `ResourceType` handle can represent a file, socket, or database connection whose destructor is controlled by the resource implementation. Dropping the handle runs the destructor after outstanding borrows and lends have ended.

```cpp
cmcpp::ResourceType file_resource(instance);
uint32_t file_rep = 17;
uint32_t file_handle = cmcpp::canon_resource_new(instance, file_resource, file_rep, trap);
uint32_t same_rep = cmcpp::canon_resource_rep(instance, file_resource, file_handle, trap);
cmcpp::canon_resource_drop(instance, file_resource, file_handle, trap);
```

**Borrow.** A borrowed `ResourceType` handle lets a call use an existing file or socket without taking ownership. The lift/lower context tracks the borrow scope and prevents the resource from being dropped while the call still uses it.

```cpp
// The WIT function signature contains borrow<file-resource>.
// The owning handle stays alive for the duration of the call.
uint32_t borrowed_rep = cmcpp::canon_resource_rep(
  instance, file_resource, file_handle, trap);
```

### Host Functions
- [x] lower_flat_values
- [x] lift_flat_values

### Tests / Samples
- [x] ABI
- [ ] WasmTime
- [x] Wamr
- [ ] WasmEdge

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

### Basic Build (Header-only Library)

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
- `BUILD_GRAMMAR` (default: ON) - Generate C++ code from ANTLR grammar for WIT parsing

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

The core library has no compiled library dependency. To use it in your project,
include `cmcpp.hpp` and either add the `include` directory to your include path
or install the CMake interface target:
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
