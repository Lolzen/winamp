cmake_minimum_required(VERSION 3.16)

# Native Linux build of Winamp (classic skin player).
# The Windows build still uses winampAll_2019.sln; see README.md.
project(winamp-linux VERSION 5.9.2 LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(FATAL_ERROR "This CMake project builds the native Linux port. Use winampAll_2019.sln on Windows.")
endif()

add_subdirectory(Src/Linux)
