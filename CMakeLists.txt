cmake_minimum_required(VERSION 3.10)
project(Winamp)

# 1. Search for the header in standard locations
# We remove the explicit /projectM subdirectory and let CMake search the standard paths
find_path(PROJECTM_INCLUDE_DIR 
    NAMES PmVisualizer.h
)

# 2. Search for the library using the generic name
find_library(PROJECTM_LIBRARY 
    NAMES projectM
)

# 3. Validate
if(NOT PROJECTM_INCLUDE_DIR OR NOT PROJECTM_LIBRARY)
    message(FATAL_ERROR "projectM library or headers not found. 
    Header search found: ${PROJECTM_INCLUDE_DIR}
    Library search found: ${PROJECTM_LIBRARY}")
endif()

# 4. Define the wrapper target
add_library(projectM_wrapper SHARED Src/Plugins/DSP/projectM_wrapper.cpp)

# 5. Use the found paths
target_include_directories(projectM_wrapper PRIVATE ${PROJECTM_INCLUDE_DIR})
target_link_libraries(projectM_wrapper PRIVATE ${PROJECTM_LIBRARY})

# Ensure the output is a .so file for the Linux plugin system
set_target_properties(projectM_wrapper PROPERTIES PREFIX "")