cmake_minimum_required(VERSION 3.10)

function(wit2code witFile)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${witFile}/${witFile}.c ${CMAKE_CURRENT_BINARY_DIR}/${witFile}/${witFile}.h ${CMAKE_CURRENT_BINARY_DIR}/${witFile}/${witFile}_component_type.o
        # COMMAND wit-bindgen markdown ${CMAKE_CURRENT_SOURCE_DIR}/${witFile}.wit
        COMMAND ${CMAKE_COMMAND} -E env PATH=$ENV{HOME}/.cargo/bin:$ENV{PATH} wit-bindgen c --out-dir "${CMAKE_CURRENT_BINARY_DIR}/${witFile}" "${CMAKE_CURRENT_SOURCE_DIR}/${witFile}.wit"
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

add_custom_command(
    TARGET sample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E env PATH=$ENV{HOME}/.cargo/bin:$ENV{PATH} wasm-tools component new ${CMAKE_CURRENT_BINARY_DIR}/sample.wasm -o ${CMAKE_CURRENT_BINARY_DIR}/sample-component.wasm
    # COMMAND npx jco transpile ${CMAKE_CURRENT_BINARY_DIR}/test.component.wasm -o ${CMAKE_CURRENT_BINARY_DIR}/js --wasi-shim --map wasi-*=@bytecodealliance/preview2-shim/* --map print=../../../../src-components/test/logger.js 
)
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
