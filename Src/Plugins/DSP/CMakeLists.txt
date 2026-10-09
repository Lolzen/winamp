cmake_minimum_required(VERSION 3.10)
project(projectM_winamp_plugin)

set(CMAKE_CXX_STANDARD 17)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Define projectM include paths and libraries
# Note: These paths should be adjusted to match the actual environment
set(PROJECTM_INCLUDE_DIR "C:/libs/projectM/include")
set(PROJECTM_LIBRARY "C:/libs/projectM/lib/projectM.lib")

add_library(projectM_wrapper SHARED 
    projectM_wrapper.cpp
)

target_include_directories(projectM_wrapper PRIVATE ${PROJECTM_INCLUDE_DIR})
target_link_libraries(projectM_wrapper 
    ${PROJECTM_LIBRARY} 
    OpenGL::GL 
    opengl32.lib
)

# Ensure the output is a .dll
set_target_properties(projectM_wrapper PROPERTIES PREFIX "" SUFFIX ".dll")