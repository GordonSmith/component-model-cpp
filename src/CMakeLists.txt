project(component-model-cpp)

# ---- Add source files ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt CONFIG REQUIRED)

set(SRC
#   component-model.cpp
    context.hpp
    context.cpp
    boolean.hpp
    float.hpp
    float.cpp
    integer.hpp
    integer.cpp
    list.hpp
    list.cpp
    load.hpp
    load.cpp
    string.hpp
    string.cpp
    traits.hpp
    traits.cpp
    variant.hpp
    variant.cpp
    util.hpp
    util.cpp
)

add_library(${PROJECT_NAME} STATIC ${SRC})

# target_include_directories(${PROJECT_NAME}
#   ...
# )

target_link_libraries(${PROJECT_NAME} 
#   PRIVATE fmt::fmt
)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
# target_compile_options(${PROJECT_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                         $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

