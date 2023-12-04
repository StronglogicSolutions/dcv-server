cmake_minimum_required(VERSION 3.6)
project(dcv_channel)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g")


include_directories(
  src
  "C:\protobuf\x64-windows\include")

link_directories(
  "C:\protobuf\x64-windows\lib")

set(SOURCES
  src/main.cpp
  src/simplelogger.c
  src/extensions.pb.cc)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} -lprotobuf)
