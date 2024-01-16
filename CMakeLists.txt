cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_COMPILER "g++-13")

project(JcTcp)

set(TARGET test)
add_executable(${TARGET} src/Test.cpp)
target_include_directories(${TARGET} PRIVATE "./inc")

