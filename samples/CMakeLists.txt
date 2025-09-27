include(ExternalProject)

set(WAMR_SAMPLE_AVAILABLE FALSE)

add_subdirectory(wamr)

if (WAMR_SAMPLE_AVAILABLE)
    if (NOT WASI_SDK_PREFIX)
        message(FATAL_ERROR "WASI_SDK_PREFIX is not set. Install wasi-sdk via vcpkg or set WASI_SDK_PREFIX manually before building the samples.")
    endif()

    if (NOT DEFINED NINJA_EXECUTABLE)
        set(_ninja_hint_dirs)
        set(_ninja_candidate_files)

        if (DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_HOST_TRIPLET)
            set(_vcpkg_tool_root "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools")
            if (EXISTS "${_vcpkg_tool_root}")
                file(GLOB _vcpkg_fetched_ninja
                    "${_vcpkg_tool_root}/ninja*/ninja.exe"
                    "${_vcpkg_tool_root}/ninja*/ninja"
                    "${_vcpkg_tool_root}/ninja*/bin/ninja.exe"
                    "${_vcpkg_tool_root}/ninja*/bin/ninja"
                )
                list(APPEND _ninja_candidate_files ${_vcpkg_fetched_ninja})
                foreach(_file IN LISTS _vcpkg_fetched_ninja)
                    get_filename_component(_dir "${_file}" DIRECTORY)
                    list(APPEND _ninja_hint_dirs "${_dir}")
                endforeach()
            endif()
        endif()

        set(_repo_vcpkg_dir "${CMAKE_SOURCE_DIR}/vcpkg")
        if (EXISTS "${_repo_vcpkg_dir}/downloads/tools")
            file(GLOB _repo_fetched_ninja
                "${_repo_vcpkg_dir}/downloads/tools/ninja*/ninja.exe"
                "${_repo_vcpkg_dir}/downloads/tools/ninja*/ninja"
                "${_repo_vcpkg_dir}/downloads/tools/ninja*/bin/ninja.exe"
                "${_repo_vcpkg_dir}/downloads/tools/ninja*/bin/ninja"
            )
            list(APPEND _ninja_candidate_files ${_repo_fetched_ninja})
            foreach(_file IN LISTS _repo_fetched_ninja)
                get_filename_component(_dir "${_file}" DIRECTORY)
                list(APPEND _ninja_hint_dirs "${_dir}")
            endforeach()
            list(APPEND _ninja_hint_dirs "${_repo_vcpkg_dir}/downloads/tools")
        endif()

        if (_ninja_hint_dirs)
            list(REMOVE_DUPLICATES _ninja_hint_dirs)
            find_program(NINJA_EXECUTABLE ninja
                HINTS ${_ninja_hint_dirs}
                NO_DEFAULT_PATH
            )
        endif()

        if (NOT NINJA_EXECUTABLE)
            foreach(_candidate IN LISTS _ninja_candidate_files)
                if (EXISTS "${_candidate}")
                    set(NINJA_EXECUTABLE "${_candidate}")
                    break()
                endif()
            endforeach()
        endif()
    endif()

    if (NOT NINJA_EXECUTABLE)
        find_program(NINJA_EXECUTABLE ninja)
    endif()

    if (NOT NINJA_EXECUTABLE)
        message(FATAL_ERROR "Ninja build tool not found. Run './vcpkg/vcpkg.exe fetch ninja' or set NINJA_EXECUTABLE manually to build the wasm guest sample.")
    endif()

    ExternalProject_Add(wasm
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wasm
        BUILD_ALWAYS TRUE
        PREFIX wasm
        CMAKE_GENERATOR Ninja
        CMAKE_ARGS
            -DHPCC_WASM_ROOT=${HPCC_WASM_ROOT}
            -DWASI_SDK_PREFIX=${WASI_SDK_PREFIX}
            -DCMAKE_TOOLCHAIN_FILE=${WASI_SDK_PREFIX}/share/cmake/wasi-sdk.cmake
            -DCMAKE_SYSROOT=${WASI_SDK_PREFIX}/share/wasi-sysroot
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_MAKE_PROGRAM=${NINJA_EXECUTABLE}
    )

    add_dependencies(wamr wasm)
endif()

add_custom_target(samples)
add_dependencies(samples wamr)
