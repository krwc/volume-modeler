add_executable(${CMAKE_PROJECT_NAME} $<TARGET_OBJECTS:${CMAKE_PROJECT_NAME}_object> main.cpp)
target_link_libraries(${CMAKE_PROJECT_NAME}
                      ${GL_LIBRARIES}
                      ${GLFW3_LIBRARIES}
                      ${GLEW_LIBRARIES}
                      ${OPENCL_LIBRARIES}
                      ${ZLIB_LIBRARIES}
                      ${Boost_LIBRARIES}
                      Threads::Threads)
