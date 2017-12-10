cmake_minimum_required(VERSION 3.1)
project(volume-modeler)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -Wextra -Wpedantic -O3 -march=native -mtune=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Wpedantic -ggdb")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(FindPkgConfig)
pkg_search_module(GL REQUIRED gl)
pkg_search_module(OPENCL REQUIRED OpenCL)
pkg_search_module(GLFW3 REQUIRED glfw3)
pkg_search_module(GLM REQUIRED glm)
pkg_search_module(GLEW REQUIRED glew)
pkg_search_module(ZLIB REQUIRED zlib)
find_package(Boost REQUIRED COMPONENTS filesystem regex iostreams)
find_package(Threads REQUIRED)

set(VM_CHUNK_SIZE 64 CACHE STRING "Size of the scene chunk (80 recommended)")
set(VM_CHUNK_BORDER 2 CACHE STRING "Chunk border improving chunk transitions quality (2 recommended)")
set(VM_VOXEL_SIZE 0.02 CACHE STRING "Size of the single voxel in chunk (0.02 recommended)")

option(WITH_TEST "Enables/disables test suite compilation" ON)
option(WITH_FEATURES "Enables/disables QEF solver" OFF)

# Used by the logger module to remove path prefix.
set(LOGGER_SOURCES_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(src)
add_subdirectory(src)
add_subdirectory(third_party)

file(GLOB_RECURSE CPP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/media DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

include(ExternalProject)
set(STB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb/")
ExternalProject_Add(stb
    SOURCE_DIR "${STB_SOURCE_DIR}"
    GIT_SUBMODULES "stb"
    INSTALL_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)
set(STB_INCLUDES ${STB_SOURCE_DIR})
include_directories(third_party)

file(GLOB_RECURSE CL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/media/kernels/*)
file(GLOB_RECURSE GLSL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/media/shaders/*)
foreach(filename ${CL_SOURCES})
    file(RELATIVE_PATH relative_filename ${CMAKE_CURRENT_SOURCE_DIR} ${filename})
    configure_file(${filename} ${CMAKE_CURRENT_BINARY_DIR}/${relative_filename})
endforeach()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/config)

if(WITH_TEST)
    add_subdirectory(test)
endif()

add_library(${PROJECT_NAME}_object OBJECT ${CPP_SOURCES})
add_subdirectory(demo)
