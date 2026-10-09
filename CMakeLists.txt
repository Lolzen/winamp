cmake_minimum_required(VERSION 3.10)
project(Winamp_ProjectM_Plugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(UNIX AND NOT APPLE)
    # Search for projectM library
    find_library(PROJECTM_LIB projectM)
    find_package(OpenGL REQUIRED)
    
    # X11 is required for glXSwapBuffers
    find_library(X11_LIB X11)

    if(NOT PROJECTM_LIB)
        message(FATAL_ERROR "libprojectM not found. Please install projectM development packages.")
    endif()

    add_library(projectM_wrapper SHARED Src/Plugins/DSP/projectM_wrapper.cpp)
    
    target_link_libraries(projectM_wrapper 
        PRIVATE 
        ${PROJECTM_LIB} 
        OpenGL::GL 
        ${X11_LIB} 
        pthread
    )

    # Ensure the output file is named correctly for Winamp to find it
    set_target_properties(projectM_wrapper PROPERTIES 
        OUTPUT_NAME "projectM_dsp"
        PREFIX ""
    )

    message(STATUS "ProjectM Linux plugin configured. Target: projectM_wrapper (.so)")
endif()