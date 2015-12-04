cmake_minimum_required(VERSION 2.8)

project(deepiot)

if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "-Wall -Wno-unused-parameter -fno-strict-aliasing -O3")
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -g -std=c++11 ${CMAKE_CXX_FLAGS}")
    add_definitions(-DGCC)
endif()

add_subdirectory(libuv)

include_directories(.
libuv/include
)

add_executable(deepiot
  http-parser/http_parser.c
  http-parser/http_parser.h
  drivers/usb/iusb.cpp
  drivers/usb/iusb.h
  drivers/tenkey.cpp
  drivers/tenkey.h
  main.cc
)

target_link_libraries(deepiot libuv)
