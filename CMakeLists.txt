cmake_minimum_required(VERSION 3.28.3)
# 同时支持C + CXX
project(mini_logger C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 导出compile_commands.json，给clangd读取编译参数【关键新增行】
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

# 头文件搜索路径
include_directories(src src/c_core src/cpp_wrapper)

# ASAN 调试选项，Release时注释这两行
add_compile_options(-fsanitize=address -g -O1)
add_link_options(-fsanitize=address)

# 只放 .c .cpp，去掉所有 .h .hpp
add_executable(mini_logger
        example/main.cpp
        src/c_core/mempool.c
        src/c_core/ringbuffer.c
        src/cpp_wrapper/logger.cpp
        "Demo(learn)/demo_offsetof.c"
        "Demo(learn)/demo_mempool_single.c"
        "Demo(learn)/demo_mempool_threadsafe.c"
        "Demo(learn)/demo_thread_exit.cpp"
        "Demo(learn)/opaque_demo/opaque_demo.c"
        "Demo(learn)/opaque_demo/opaque_demo.h"
        "Demo(learn)/opaque_demo/main.cpp"
        "Demo(learn)/demo_atomic.c"
)

# 链接线程库
target_link_libraries(mini_logger pthread)
