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
- [ ] Enum
- [x] Option
- [ ] Result
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

## Usage

This library is a header only library. To use it in your project, you can:
- [x] Copy the contents of the `include` directory to your project.
- [ ] Use `vcpkg` to install the library and its dependencies.

 
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
