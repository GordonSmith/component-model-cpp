cmake_minimum_required(VERSION 3.22)

project(wasmtests)

set(WASM_PATH "${CMAKE_CURRENT_BINARY_DIR}/bin/${PROJECT_NAME}.wasm")

add_custom_command(
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/test.wit
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests.c ${CMAKE_CURRENT_BINARY_DIR}/tests.h ${CMAKE_CURRENT_BINARY_DIR}/tests_component_type.o
    COMMAND wit-bindgen c --out-dir ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test.wit
)
add_custom_target(wit-generate ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tests.c)

set(CMAKE_EXECUTABLE_SUFFIX ".wasm")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions --sysroot=/${WASI_SDK_PREFIX}/share/wasi-sysroot")

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(wasmtests
    bool.cpp
    int.cpp
    float.cpp
    list.cpp
    string.cpp
    uint.cpp
    util.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/tests.c
    ${CMAKE_CURRENT_BINARY_DIR}/tests.h
)

target_link_options(wasmtests PRIVATE "LINKER:--no-entry")


add_custom_command(
    TARGET wasmtests POST_BUILD  
    # OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/wasmtests-component.wasm
    COMMAND echo "wasm-tools component new ${CMAKE_CURRENT_BINARY_DIR}/wasmtests.wasm -o ${CMAKE_CURRENT_BINARY_DIR}/wasmtests-component.wasm --adapt wasi_snapshot_preview1=${WASI_SDK_PREFIX}/wasi_snapshot_preview1.reactor.wasm"
)

target_link_libraries(wasmtests
    ${CMAKE_CURRENT_BINARY_DIR}/tests_component_type.o
)

install(TARGETS wasmtests
    RUNTIME DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/../install-target
)
