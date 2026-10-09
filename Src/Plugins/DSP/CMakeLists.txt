cmake_minimum_required(VERSION 3.10)
project(projectM_winamp_plugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Platform-specific configuration
if(WIN32)
    # Windows settings
    set(PROJECTM_INCLUDE_DIR "C:/libs/projectM/include") # Adjust to your local path
    set(PROJECTM_LIBRARY "C:/libs/projectM/lib/projectM.lib") # Adjust to your local path
    set(PLATFORM_LIBS opengl32.lib)
else()
    # Linux settings
    # Assuming projectM is installed via package manager (e.g., libprojectm-dev)
    set(PLATFORM_LIBS GL X11) 
endif()

add_library(projectM_wrapper SHARED 
    projectM_wrapper.cpp
)

target_include_directories(projectM_wrapper PRIVATE 
    ${PROJECTM_INCLUDE_DIR}
)

target_link_libraries(projectM_wrapper 
    ${PROJECTM_LIBRARY} 
    OpenGL::GL 
    ${PLATFORM_LIBS}
)

# Ensure the output is a .dll on Windows and .so on Linux
if(WIN32)
    set_target_properties(projectM_wrapper PROPERTIES PREFIX "" SUFFIX ".dll")
endif()