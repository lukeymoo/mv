cmake_minimum_required(VERSION 3.10)

 project(mvEngine VERSION 1.0
            DESCRIPTION "Vulkan with OOP"
            LANGUAGES CXX)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ./build)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(HEADERS
    Headers/mvDevice.h
    Headers/mvKeyboard.h
    Headers/mvMouse.h
    Headers/mvException.h
    Headers/mvWindow.h)

set(SOURCES
    mvDevice.cpp
    mvKeyboard.cpp
    mvMouse.cpp
    mvException.cpp
    mvWindow.cpp
    main.cpp)

add_compile_options(-Wall -std=c++20 -m64 -O2 -g)

add_executable(main ${HEADERS} ${SOURCES})

target_include_directories(main PUBLIC Headers/)

target_link_libraries(main gcc vulkan dl pthread X11 Xxf86vm Xrandr Xi stdc++fs)