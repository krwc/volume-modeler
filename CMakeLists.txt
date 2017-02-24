cmake_minimum_required(VERSION 2.8)
project(volume-renderer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -Wextra -Wpedantic -O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Wpedantic -ggdb")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(FindPkgConfig)
pkg_search_module(GL REQUIRED gl)
pkg_search_module(GLFW3 REQUIRED glfw3)

file(GLOB_RECURSE CPP_SOURCES src/*.cpp)

add_executable(${PROJECT_NAME} ${CPP_SOURCES})
target_link_libraries(${PROJECT_NAME} ${GL_LIBRARIES} ${GLFW3_LIBRARIES})
