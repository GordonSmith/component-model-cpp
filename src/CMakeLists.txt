project(component-model-cpp)

# ---- Add source files ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt CONFIG REQUIRED)

set(SRC
  traits.hpp
  traits.cpp
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
)

add_library(${PROJECT_NAME} SHARED ${SRC})
target_link_libraries(${PROJECT_NAME} 
  PRIVATE fmt::fmt
)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                         $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

