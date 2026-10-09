cmake_minimum_required(VERSION 3.10)
project(Winamp)

# 1. Manually search for the projectM header and library
# We add /usr/include and /usr/local/include to cover common header locations
find_path(PROJECTM_INCLUDE_DIR 
    NAMES PmVisualizer.h 
    PATHS /usr/include/projectM /usr/local/include/projectM /usr/include /usr/local/include
)

# We explicitly include /lib64 as seen in your screenshots
find_library(PROJECTM_LIBRARY 
    NAMES projectM 
    PATHS /lib64 /usr/lib64 /usr/lib /usr/local/lib
)

# 2. Validate that we actually found them
if(NOT PROJECTM_INCLUDE_DIR OR NOT PROJECTM_LIBRARY)
    message(FATAL_ERROR "projectM library or headers not found. 
    Headers searched in: ${PROJECTM_INCLUDE_DIR}
    Library searched in: ${PROJECTM_LIBRARY}")
endif()

# 3. Define the wrapper target
add_library(projectM_wrapper SHARED Src/Plugins/DSP/projectM_wrapper.cpp)

# 4. Use the found paths
target_include_directories(projectM_wrapper PRIVATE ${PROJECTM_INCLUDE_DIR})
target_link_libraries(projectM_wrapper PRIVATE ${PROJECTM_LIBRARY})

# Ensure the output is a .so file for the Linux plugin system
set_target_properties(projectM_wrapper PROPERTIES PREFIX "")