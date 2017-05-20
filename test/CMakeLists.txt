file(GLOB_RECURSE TEST_SOURCES *.cpp)

include_directories(${GTEST_INCLUDE_DIRS})

add_executable(${CMAKE_PROJECT_NAME}_test
               ${TEST_SOURCES}
               $<TARGET_OBJECTS:${CMAKE_PROJECT_NAME}_object>)
target_link_libraries(${CMAKE_PROJECT_NAME}_test
                      gtest
                      gtest_main
                      ${OPENCL_LIBRARIES}
                      ${GL_LIBRARIES}
                      ${GLEW_LIBRARIES}
                      ${OPENCL_LIBRARIES}
                      ${ZLIB_LIBRARIES}
                      ${Boost_LIBRARIES}
                      Threads::Threads)
