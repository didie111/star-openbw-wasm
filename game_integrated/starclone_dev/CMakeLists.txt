cmake_minimum_required(VERSION 3.20)
project(starclone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(starclone
  src/main.cpp
  src/ui_custom.h
)

if(WIN32)
  set_target_properties(starclone PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif()

