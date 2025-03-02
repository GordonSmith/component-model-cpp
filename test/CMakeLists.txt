project(component-model-test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(--coverage)
    add_link_options(--coverage)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
endif()

find_package(doctest CONFIG REQUIRED)
find_package(ICU REQUIRED COMPONENTS uc dt in io)

include_directories(
    ${ROOT_DIR}/src
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
  PRIVATE component-model-cpp
  PRIVATE ${ICU_LIBRARIES}
)

add_test(
  NAME ${PROJECT_NAME}
  COMMAND $<TARGET_FILE:${PROJECT_NAME}>
)

