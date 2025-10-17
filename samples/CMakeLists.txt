cmake_minimum_required(VERSION 3.10)

add_subdirectory(wasm-guests)
add_subdirectory(wamr-hosts)

add_custom_target(samples)

if (TARGET wasm-guests)
    add_dependencies(samples wasm-guests)
endif()

if (TARGET wamr-hosts)
    add_dependencies(samples wamr-hosts)
endif()
