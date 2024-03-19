[![Actions Status](https://github.com/GordonSmith/component-model-cpp/workflows/MacOS/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions)
[![Actions Status](https://github.com/GordonSmith/component-model-cpp/workflows/Windows/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions)
[![Actions Status](https://github.com/GordonSmith/component-model-cpp/workflows/Ubuntu/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions)
[![Actions Status](https://github.com/GordonSmith/component-model-cpp/workflows/Style/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions)
[![Actions Status](https://github.com/GordonSmith/component-model-cpp/workflows/Install/badge.svg)](https://github.com/GordonSmith/component-model-cpp/actions)
[![codecov](https://codecov.io/gh/GordonSmith/component-model-cpp/branch/master/graph/badge.svg)](https://codecov.io/gh/GordonSmith/component-model-cpp)

<p align="center">
  <img src="https://github.com/WebAssembly/WASI/blob/main/WASI.png?raw=true" height="175" width="auto" />
  <img src="https://repository-images.githubusercontent.com/254842585/4dfa7580-7ffb-11ea-99d0-46b8fe2f4170" height="175" width="auto" />
</p>

# Component Model C++

This repository contains a C++ ABI implementation of the WebAssembly Component Model.

## Features

### OS
- [x] Linux
- [ ] MacOS
- [ ] Windows

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
- [x] Float32
- [x] Float64
- [x] Char
- [x] String
- [x] utf8 String
- [ ] utf16 String
- [x] List
- [x] Field
- [x] Record
- [x] Tuple
- [x] Case
- [x] Variant
- [x] Enum
- [x] Option
- [x] Result
- [x] Flags
- [ ] Own
- [ ] Borrow

### Host Functions
- [x] lower_values
- [x] lift_values

### Tests
- [x] ABI
- [ ] WasmTime
- [ ] Wamr
- [ ] WasmEdge

## Usage

TODO

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

