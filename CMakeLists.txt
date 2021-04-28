cmake_minimum_required(VERSION 3.10)

 project(mvEngine VERSION 1.0
            DESCRIPTION "Vulkan with OOP"
            LANGUAGES CXX)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ./build)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(HEADERS
    Headers/mvAllocator.h
    Headers/mvTimer.h
    Headers/mvCamera.h
    Headers/mvModel.h
    Headers/mvBuffer.h
    Headers/mvEngine.h
    Headers/mvInit.h
    Headers/mvSwap.h
    Headers/mvDevice.h
    Headers/mvKeyboard.h
    Headers/mvMouse.h
    Headers/mvException.h
    Headers/mvWindow.h)

set(SOURCES
    mvTimer.cpp
    mvCamera.cpp
    mvModel.cpp
    mvBuffer.cpp
    mvEngine.cpp
    mvSwap.cpp
    mvDevice.cpp
    mvKeyboard.cpp
    mvMouse.cpp
    mvException.cpp
    mvWindow.cpp
    main.cpp)

add_compile_options(-Wall -std=c++17 -m64 -O3 -g)

add_executable(main ${HEADERS} ${SOURCES})

target_include_directories(main PUBLIC Headers/)

target_link_libraries(main gcc vulkan dl X11 Xxf86vm Xrandr Xi stdc++fs)