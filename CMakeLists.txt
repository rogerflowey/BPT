cmake_minimum_required(VERSION 3.28)
project(BPT)

set(CMAKE_CXX_STANDARD 23)

include_directories(.)


add_executable(BPT
        src/BPT.cpp
        src/disk/IO_manager.cpp
        src/disk/IO_utils.cpp
)
