cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(SYNTIANT LANGUAGES CXX)

include(../../build/BuildDefaults.cmake)
include_directories("include")
add_subdirectory("src")
add_subdirectory("test")
