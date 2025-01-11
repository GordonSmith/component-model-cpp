project(component-model-test)

find_package(doctest CONFIG REQUIRED)
find_package(ICU REQUIRED COMPONENTS uc i18n io)

include_directories(
    ../src
    ${ICU_INCLUDE_DIR}
)

add_executable(${PROJECT_NAME} 
  main.cpp
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

