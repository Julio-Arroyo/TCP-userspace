cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_COMPILER "g++")

project(JcTcp)

option(CLIENT_SERVER_DEMO "Build basic Client-Server demo" OFF)
option(FLOW_COMPLETION_TIME "Build the Flow Completion Time experiment" OFF)
if (NOT CLIENT_SERVER_DEMO AND NOT FLOW_COMPLETION_TIME)
  message("-- Did not specify examples to be built; building all examples.")
  set(CLIENT_SERVER_DEMO ON)
  set(FLOW_COMPLETION_TIME ON)
endif()

set(JC_TCP_SOURCES src/JcTcpPacket.cpp
                   src/JcTcpSocket.cpp
                   src/JcTcpSocketBackend.cpp
                   src/Util.cpp)

if (CLIENT_SERVER_DEMO)
  set(TARGET_CLIENT demo_client)
  add_executable(${TARGET_CLIENT} ${JC_TCP_SOURCES} examples/Demo/Client.cpp)
  target_include_directories(${TARGET_CLIENT} PRIVATE "./inc")

  set(TARGET_SERVER demo_server)
  add_executable(${TARGET_SERVER} ${JC_TCP_SOURCES} examples/Demo/Server.cpp)
  target_include_directories(${TARGET_SERVER} PRIVATE "./inc")
endif()

if (FLOW_COMPLETION_TIME)
  set(TARGET_FCT_CLIENT fct_client)
  add_executable(${TARGET_FCT_CLIENT} ${JC_TCP_SOURCES} examples/FlowCompletionTime/Client.cpp)
  target_include_directories(${TARGET_FCT_CLIENT} PRIVATE "./inc")

  set(TARGET_FCT_SERVER fct_server)
  add_executable(${TARGET_FCT_SERVER} ${JC_TCP_SOURCES} examples/FlowCompletionTime/Server.cpp)
  target_include_directories(${TARGET_FCT_SERVER} PRIVATE "./inc")
endif()

