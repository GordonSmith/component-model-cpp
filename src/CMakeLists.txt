project(component-model-cpp)

# ---- Add source files ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC
  context.hpp
  context.cpp
  flatten.hpp
  flatten.cpp
  lift.hpp
  lift.cpp
  load.hpp
  load.cpp
  lower.hpp
  lower.cpp
  store.hpp
  store.cpp
  util.hpp
  util.cpp
  val.hpp
  val.cpp
  val2.hpp
  val2.cpp
)

add_library(${PROJECT_NAME} SHARED ${SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                         $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

