cmake_minimum_required(VERSION 3.10)

if(WIN32)
    if(DEFINED ENV{USERPROFILE} AND NOT "$ENV{USERPROFILE}" STREQUAL "")
        set(_cargo_bin_root "$ENV{USERPROFILE}")
    else()
        set(_cargo_bin_root "$ENV{HOME}")
    endif()
    if(NOT "${_cargo_bin_root}" STREQUAL "")
        set(_cargo_bin "${_cargo_bin_root}\\.cargo\\bin")
    else()
        set(_cargo_bin "")
    endif()
    if(DEFINED ENV{PATH})
        string(REPLACE ";" "\\;" _escaped_env_path "$ENV{PATH}")
    else()
        set(_escaped_env_path "")
    endif()
    if(NOT "${_cargo_bin}" STREQUAL "")
        if(NOT "${_escaped_env_path}" STREQUAL "")
            set(CARGO_BIN_ENV "PATH=${_cargo_bin}\\;${_escaped_env_path}")
        else()
            set(CARGO_BIN_ENV "PATH=${_cargo_bin}")
        endif()
    else()
        set(CARGO_BIN_ENV "PATH=${_escaped_env_path}")
    endif()
else()
    set(_cargo_bin "$ENV{HOME}/.cargo/bin")
    if(DEFINED ENV{PATH})
        set(_escaped_env_path "$ENV{PATH}")
    else()
        set(_escaped_env_path "")
    endif()
    if(NOT "${_cargo_bin}" STREQUAL "")
        if(NOT "${_escaped_env_path}" STREQUAL "")
            set(CARGO_BIN_ENV "PATH=${_cargo_bin}:${_escaped_env_path}")
        else()
            set(CARGO_BIN_ENV "PATH=${_cargo_bin}")
        endif()
    else()
        set(CARGO_BIN_ENV "PATH=${_escaped_env_path}")
    endif()
endif()

function(wit2code witFile)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${witFile}/${witFile}.c ${CMAKE_CURRENT_BINARY_DIR}/${witFile}/${witFile}.h ${CMAKE_CURRENT_BINARY_DIR}/${witFile}/${witFile}_component_type.o
        # COMMAND wit-bindgen markdown ${CMAKE_CURRENT_SOURCE_DIR}/${witFile}.wit
        COMMAND ${CMAKE_COMMAND} -E env "${CARGO_BIN_ENV}" wit-bindgen c --out-dir "${CMAKE_CURRENT_BINARY_DIR}/${witFile}" "${CMAKE_CURRENT_SOURCE_DIR}/${witFile}.wit"
        # COMMAND wit-bindgen rust --ownership owning --out-dir ${CMAKE_CURRENT_BINARY_DIR}/${witFile}/rust ${CMAKE_CURRENT_SOURCE_DIR}/${witFile}.wit
        # COMMAND wit-bindgen teavm-java --out-dir ${CMAKE_CURRENT_BINARY_DIR}/${witFile}/java ${CMAKE_CURRENT_SOURCE_DIR}/${witFile}.wit
        # COMMAND wit-bindgen tiny-go --out-dir ${CMAKE_CURRENT_BINARY_DIR}/${witFile}/go ${CMAKE_CURRENT_SOURCE_DIR}/${witFile}.wit
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    add_custom_target(wit-generate-${witFile} ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${witFile}/${witFile}.c
                ${CMAKE_CURRENT_BINARY_DIR}/${witFile}/${witFile}.h
                ${CMAKE_CURRENT_BINARY_DIR}/${witFile}/${witFile}_component_type.o
    )
endfunction()

project(sample)

# add_definitions(-D_WASI_EMULATED_SIGNAL)
# add_link_options(-lwasi-emulated-signal)

# set(CMAKE_TOOLCHAIN_FILE "${WASI_SDK_PREFIX}/share/cmake/wasi-sdk.cmake")
# set(CMAKE_SYSROOT "${WASI_SDK_PREFIX}/share/wasi-sysroot")

set(CMAKE_EXECUTABLE_SUFFIX ".wasm")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".wasm")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostartfiles -fno-exceptions --sysroot=${WASI_SDK_PREFIX}/share/wasi-sysroot")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-entry -Wl,--export-all")

set(CMAKE_EXECUTABLE_SUFFIX ".wasm")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostartfiles -fno-exceptions --sysroot=${WASI_SDK_PREFIX}/share/wasi-sysroot -Wl,--no-entry")

wit2code(sample)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}/sample
)

add_executable(sample
    main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/sample/sample.c 
    ${CMAKE_CURRENT_BINARY_DIR}/sample/sample.h 
)

target_link_libraries(sample
    ${CMAKE_CURRENT_BINARY_DIR}/sample/sample_component_type.o
)

if (NOT DEFINED WASM_TOOLS_EXECUTABLE)
    if (WIN32)
        find_program(WASM_TOOLS_EXECUTABLE wasm-tools HINTS "${_cargo_bin}")
    else()
        find_program(WASM_TOOLS_EXECUTABLE wasm-tools HINTS "${_cargo_bin}")
    endif()
endif()

if (WASM_TOOLS_EXECUTABLE)
    add_custom_command(
        TARGET sample POST_BUILD
        COMMAND ${WASM_TOOLS_EXECUTABLE} component new ${CMAKE_CURRENT_BINARY_DIR}/sample.wasm -o ${CMAKE_CURRENT_BINARY_DIR}/sample-component.wasm
        # COMMAND npx jco transpile ${CMAKE_CURRENT_BINARY_DIR}/test.component.wasm -o ${CMAKE_CURRENT_BINARY_DIR}/js --wasi-shim --map wasi-*=@bytecodealliance/preview2-shim/* --map print=../../../../src-components/test/logger.js 
    )
else()
    message(WARNING "wasm-tools not found; skipping component conversion. Install via 'cargo install wasm-tools' to produce sample-component.wasm.")
endif()
# set_property(SOURCE main.cpp APPEND PROPERTY OBJECT_DEPENDS ${HPCC_WASM_ROOT}/build/guest/cpp/wit/test.c)

# Install the target and export the target information
install(TARGETS sample
    EXPORT addConfig
    RUNTIME DESTINATION bin
)

# Export the target information to a file
install(EXPORT addConfig
    FILE addConfig.cmake
    NAMESPACE sample::
    DESTINATION lib/cmake/sample
)
