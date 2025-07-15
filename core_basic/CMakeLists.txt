cmake_minimum_required(VERSION 3.16)
project(CoroExample LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置C++20标准（必需协程支持）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SRC_FILES "*.cc")

foreach(src_file ${SRC_FILES})
    get_filename_component(exec_name ${src_file} NAME_WE)

    # 添加可执行文件
    add_executable(${exec_name} ${src_file})

    target_link_libraries(${exec_name} PUBLIC pthread uring)
        
    # 检查GCC版本（协程需要g++>=10.1）
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.1)
        message(FATAL_ERROR "GCC version must be at least 10.1 for coroutine support")
    endif()
endforeach()
