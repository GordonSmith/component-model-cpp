#!/bin/bash
cd "$(dirname "$0")"
mkdir -p ../../build
cd ../../build
cmake ..
cmake --build . --target wasmtime --parallel
