cmake_minimum_required(VERSION 3.10)
project(Winamp)

# ... (existing setup, dependencies, etc) ...

# 1. Find PkgConfig to locate projectM
find_package(PkgConfig REQUIRED)
pkg_check_modules(PROJECTM REQUIRED projectM)

# 2. Define the wrapper target
add_library(projectM_wrapper SHARED Src/Plugins/DSP/projectM_wrapper.cpp)

# 3. FIX: Explicitly add the include directories found by pkg-config
# This resolves the "fatal error: projectM/PmVisualizer.h: No such file"
target_include_directories(projectM_wrapper PRIVATE ${PROJECTM_INCLUDE_DIRS})

# 4. Link the actual projectM libraries
target_link_libraries(projectM_wrapper PRIVATE ${PROJECTM_LIBRARIES})

# Ensure the output is a .so file for the Linux plugin system
set_target_properties(projectM_wrapper PROPERTIES PREFIX "")