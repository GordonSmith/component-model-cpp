project(unit-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(--coverage)
    add_link_options(--coverage)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
endif()

# Suppress narrowing conversion warnings on MSVC for WebAssembly 32-bit ABI
if(MSVC)
    add_compile_options(
        /wd4244  # conversion from 'type1' to 'type2', possible loss of data
        /wd4267  # conversion from 'size_t' to 'type', possible loss of data
        /wd4305  # truncation from 'type1' to 'type2'
        /wd4309  # truncation of constant value
    )
endif()

find_package(doctest CONFIG REQUIRED)
find_package(ICU REQUIRED COMPONENTS uc dt in io)

include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${ICU_INCLUDE_DIR}
)

add_executable(${PROJECT_NAME} 
  main.cpp
  scratchpad.cpp
  host-util.hpp
  host-util.cpp
)

target_link_libraries(${PROJECT_NAME} 
  PRIVATE doctest::doctest
  PRIVATE cmcpp
  PRIVATE ${ICU_LIBRARIES}
)

add_test(
  NAME ${PROJECT_NAME}
  COMMAND $<TARGET_FILE:${PROJECT_NAME}>
)
