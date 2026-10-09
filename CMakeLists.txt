cmake_minimum_required(VERSION 3.10)
project(Winamp)

# 1. Search for the projectM header
# We search for projectM.hpp because that's what exists on this system
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
# We add the parent directory (/usr/include) so that <projectM/...> 
# can potentially resolve if there's a naming overlap, 
# but specifically we include the path where the headers actually live.
target_include_directories(projectM_wrapper PRIVATE 
    ${PROJECTM_INCLUDE_DIR}
    /usr/include
)
target_link_libraries(projectM_wrapper PRIVATE ${PROJECTM_LIBRARY})

# Ensure the output is a .so file for the Linux plugin system
set_target_properties(projectM_wrapper PROPERTIES PREFIX "")