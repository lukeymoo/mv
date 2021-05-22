cmake_minimum_required(VERSION 3.10)

 project(mvEngine VERSION 1.0
            DESCRIPTION "Vulkan with OOP"
            LANGUAGES CXX)

set(CMAKE_C_COMPILER "gcc-11")
set(CMAKE_CXX_COMPILER "g++-11")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ./build)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(X11 REQUIRED)

set(HEADERS
    Headers/imgui-1.82/imgui.h
    Headers/imgui-1.82/backends/imgui_impl_glfw.h
    Headers/imgui-1.82/backends/imgui_impl_vulkan.h
    Headers/mvMap.h
    Headers/mvGui.h
    Headers/mvHelper.h
    Headers/mvInput.h
    Headers/mvWindow.h
    Headers/mvCollection.h
    Headers/mvImage.h
    Headers/mvAllocator.h
    Headers/mvTimer.h
    Headers/mvCamera.h
    Headers/mvModel.h
    Headers/mvBuffer.h
    Headers/mvEngine.h
    Headers/mvSwap.h)

set(SOURCES
    Headers/imgui-1.82/backends/imgui_impl_glfw.cpp
    Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp
    Headers/imgui-1.82/imgui_tables.cpp
    Headers/imgui-1.82/imgui_widgets.cpp
    Headers/imgui-1.82/imgui_draw.cpp
    Headers/imgui-1.82/imgui_demo.cpp
    Headers/imgui-1.82/imgui.cpp
    mvMap.cpp
    mvGui.cpp
    mvHelper.cpp
    mvCollection.cpp
    mvAllocator.cpp
    mvInput.cpp
    mvImage.cpp
    mvTimer.cpp
    mvCamera.cpp
    mvModel.cpp
    mvBuffer.cpp
    mvEngine.cpp
    mvSwap.cpp
    mvWindow.cpp
    main.cpp)

add_compile_options(-Wall -Wextra -std=c++20 -O0 -m64 -g)

add_executable(main ${HEADERS} ${SOURCES})

target_include_directories(main PUBLIC Headers/ Headers/imgui-1.82/ Headers/imgui-1.82/backends/)

target_link_libraries(main gcc vulkan dl stdc++fs assimp glfw)