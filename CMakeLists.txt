cmake_minimum_required(VERSION 3.10)
project(minidb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
add_compile_options(-Wall -Wextra -O2)

# 源文件
set(COMMON_SOURCES
    src/server_main.cpp
)

set(CLIENT_SOURCES
    src/client_main.cpp
)

# 头文件路径 - 包含所有子目录
include_directories(src)
include_directories(src/common)
include_directories(src/index)
include_directories(src/storage)
include_directories(src/parser)
include_directories(src/executor)
include_directories(src/network)
include_directories(tests)

# 服务端
add_executable(server ${COMMON_SOURCES})
target_link_libraries(server pthread)

# 客户端（使用 GNU Readline 支持历史命令）
add_executable(client ${CLIENT_SOURCES})
target_link_libraries(client pthread readline)

# 单元测试
add_executable(test_minidb tests/test_main.cpp)
target_link_libraries(test_minidb pthread)
