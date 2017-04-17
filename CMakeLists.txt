cmake_minimum_required(VERSION 2.8)
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
find_package(Boost REQUIRED)

set(VM_CHUNK_SIZE 128 CACHE STRING "Size of the scene chunk (128 recommended)")
set(VM_CHUNK_BORDER 4 CACHE STRING "Chunk border improving chunk transitions quality (4 recommended)")
set(VM_VOXEL_SIZE 0.02 CACHE STRING "Size of the single voxel in chunk (0.02 recommended)")
set(VM_CHUNKS_PER_PASS 16 CACHE STRING "Maximum number of chunks rendered in a single pass")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/config)

include_directories(src)
add_subdirectory(src)

file(GLOB_RECURSE CPP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(${PROJECT_NAME} ${CPP_SOURCES})
target_link_libraries(${PROJECT_NAME} ${GL_LIBRARIES} ${OPENCL_LIBRARIES} ${GLFW3_LIBRARIES} ${GLEW_LIBRARIES})
