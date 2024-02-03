cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_COMPILER "g++-13")

project(JcTcp)

set(JC_TCP_SOURCES src/JcTcpPacket.cpp
                   src/JcTcpSocket.cpp
                   src/JcTcpSocketBackend.cpp
                   src/Util.cpp)

set(TARGET_CLIENT client)
add_executable(${TARGET_CLIENT} ${JC_TCP_SOURCES} src/Client.cpp)
target_include_directories(${TARGET_CLIENT} PRIVATE "./inc")

set(TARGET_SERVER server)
add_executable(${TARGET_SERVER} ${JC_TCP_SOURCES} src/Server.cpp)
target_include_directories(${TARGET_SERVER} PRIVATE "./inc")

