cmake_minimum_required(VERSION 2.8)
project(volume-modeler)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -Wextra -Wpedantic -O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Wpedantic -ggdb")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(FindPkgConfig)
pkg_search_module(GL REQUIRED gl)
pkg_search_module(GLFW3 REQUIRED glfw3)
pkg_search_module(GLM REQUIRED glm)

set(VM_CHUNK_SIZE 256 CACHE STRING "Size of the scene chunk (256 recommended)")
set(VM_VOXEL_SIZE 0.009 CACHE STRING "Size of the single voxel in chunk (0.009 recommended)")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/config)

include_directories(src)
add_subdirectory(src)

file(GLOB_RECURSE CPP_SOURCES src/*.cpp)

add_executable(${PROJECT_NAME} ${CPP_SOURCES})
target_link_libraries(${PROJECT_NAME} ${GL_LIBRARIES} ${GLFW3_LIBRARIES})
