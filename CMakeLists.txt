cmake_minimum_required(VERSION 3.10)
project(Winamp)

# 1. Search for the projectM header
# We search for projectM.hpp inside libprojectM
find_path(PROJECTM_INCLUDE_DIR 
    NAMES projectM.hpp
    PATHS /usr/include/libprojectM /usr/local/include/libprojectM
)

# 2. Search for the library
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
# We include the specific directory where projectM.hpp lives
target_include_directories(projectM_wrapper PRIVATE ${PROJECTM_INCLUDE_DIR})
target_link_libraries(projectM_wrapper PRIVATE ${PROJECTM_LIBRARY})

# Ensure the output is a .so file for the Linux plugin system
set_target_properties(projectM_wrapper PROPERTIES PREFIX "")