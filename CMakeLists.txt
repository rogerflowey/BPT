cmake_minimum_required(VERSION 3.28)
project(BPT)

set(CMAKE_CXX_STANDARD 23)

include_directories(.)


add_executable(code
        code.cpp
        src/disk/IO_manager.cpp
        src/disk/IO_utils.cpp
)

add_executable(node_test
        test/node_test.cpp
        src/disk/IO_manager.cpp
        src/disk/IO_utils.cpp
)

add_executable(IO_test
        test/IO_test.cpp
        src/disk/IO_manager.cpp
        src/disk/IO_utils.cpp
)

add_executable(BPT_find_insert_easy_test
        test/BPT_find_insert_easy_test.cpp
        src/disk/IO_manager.cpp
        src/disk/IO_utils.cpp
)

add_executable(BPT_erase_test
        test/BPT_erase_test.cpp
        src/disk/IO_manager.cpp
        src/disk/IO_utils.cpp
)