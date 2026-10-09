cmake_minimum_required(VERSION 3.21)
project(vis_projectm LANGUAGES CXX)

find_package(projectM4 CONFIG REQUIRED)

add_library(vis_projectm MODULE vis_projectm.cpp)
target_compile_features(vis_projectm PRIVATE cxx_std_17)
target_include_directories(vis_projectm PRIVATE ../../../Winamp)
target_link_libraries(vis_projectm PRIVATE libprojectM::projectM opengl32 gdi32 user32)
set_target_properties(vis_projectm PROPERTIES PREFIX "")
