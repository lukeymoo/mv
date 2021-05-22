# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.18

# Default target executed when no arguments are given to make.
default_target: all

.PHONY : default_target

# Allow only one "make -f Makefile2" at a time, but pass parallelism.
.NOTPARALLEL:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/luke/Documents/mv

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/luke/Documents/mv

#=============================================================================
# Targets provided globally by CMake.

# Special rule for the target rebuild_cache
rebuild_cache:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Running CMake to regenerate build system..."
	/usr/bin/cmake --regenerate-during-build -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
.PHONY : rebuild_cache

# Special rule for the target rebuild_cache
rebuild_cache/fast: rebuild_cache

.PHONY : rebuild_cache/fast

# Special rule for the target edit_cache
edit_cache:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "No interactive CMake dialog available..."
	/usr/bin/cmake -E echo No\ interactive\ CMake\ dialog\ available.
.PHONY : edit_cache

# Special rule for the target edit_cache
edit_cache/fast: edit_cache

.PHONY : edit_cache/fast

# The main all target
all: cmake_check_build_system
	$(CMAKE_COMMAND) -E cmake_progress_start /home/luke/Documents/mv/CMakeFiles /home/luke/Documents/mv//CMakeFiles/progress.marks
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 all
	$(CMAKE_COMMAND) -E cmake_progress_start /home/luke/Documents/mv/CMakeFiles 0
.PHONY : all

# The main clean target
clean:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 clean
.PHONY : clean

# The main clean target
clean/fast: clean

.PHONY : clean/fast

# Prepare targets for installation.
preinstall: all
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 preinstall
.PHONY : preinstall

# Prepare targets for installation.
preinstall/fast:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 preinstall
.PHONY : preinstall/fast

# clear depends
depend:
	$(CMAKE_COMMAND) -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 1
.PHONY : depend

#=============================================================================
# Target rules for targets named main

# Build rule for target.
main: cmake_check_build_system
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 main
.PHONY : main

# fast build rule for target.
main/fast:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/build
.PHONY : main/fast

Headers/imgui-1.82/backends/imgui_impl_glfw.o: Headers/imgui-1.82/backends/imgui_impl_glfw.cpp.o

.PHONY : Headers/imgui-1.82/backends/imgui_impl_glfw.o

# target to build an object file
Headers/imgui-1.82/backends/imgui_impl_glfw.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/backends/imgui_impl_glfw.cpp.o
.PHONY : Headers/imgui-1.82/backends/imgui_impl_glfw.cpp.o

Headers/imgui-1.82/backends/imgui_impl_glfw.i: Headers/imgui-1.82/backends/imgui_impl_glfw.cpp.i

.PHONY : Headers/imgui-1.82/backends/imgui_impl_glfw.i

# target to preprocess a source file
Headers/imgui-1.82/backends/imgui_impl_glfw.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/backends/imgui_impl_glfw.cpp.i
.PHONY : Headers/imgui-1.82/backends/imgui_impl_glfw.cpp.i

Headers/imgui-1.82/backends/imgui_impl_glfw.s: Headers/imgui-1.82/backends/imgui_impl_glfw.cpp.s

.PHONY : Headers/imgui-1.82/backends/imgui_impl_glfw.s

# target to generate assembly for a file
Headers/imgui-1.82/backends/imgui_impl_glfw.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/backends/imgui_impl_glfw.cpp.s
.PHONY : Headers/imgui-1.82/backends/imgui_impl_glfw.cpp.s

Headers/imgui-1.82/backends/imgui_impl_vulkan.o: Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp.o

.PHONY : Headers/imgui-1.82/backends/imgui_impl_vulkan.o

# target to build an object file
Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp.o
.PHONY : Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp.o

Headers/imgui-1.82/backends/imgui_impl_vulkan.i: Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp.i

.PHONY : Headers/imgui-1.82/backends/imgui_impl_vulkan.i

# target to preprocess a source file
Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp.i
.PHONY : Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp.i

Headers/imgui-1.82/backends/imgui_impl_vulkan.s: Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp.s

.PHONY : Headers/imgui-1.82/backends/imgui_impl_vulkan.s

# target to generate assembly for a file
Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp.s
.PHONY : Headers/imgui-1.82/backends/imgui_impl_vulkan.cpp.s

Headers/imgui-1.82/imgui.o: Headers/imgui-1.82/imgui.cpp.o

.PHONY : Headers/imgui-1.82/imgui.o

# target to build an object file
Headers/imgui-1.82/imgui.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui.cpp.o
.PHONY : Headers/imgui-1.82/imgui.cpp.o

Headers/imgui-1.82/imgui.i: Headers/imgui-1.82/imgui.cpp.i

.PHONY : Headers/imgui-1.82/imgui.i

# target to preprocess a source file
Headers/imgui-1.82/imgui.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui.cpp.i
.PHONY : Headers/imgui-1.82/imgui.cpp.i

Headers/imgui-1.82/imgui.s: Headers/imgui-1.82/imgui.cpp.s

.PHONY : Headers/imgui-1.82/imgui.s

# target to generate assembly for a file
Headers/imgui-1.82/imgui.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui.cpp.s
.PHONY : Headers/imgui-1.82/imgui.cpp.s

Headers/imgui-1.82/imgui_demo.o: Headers/imgui-1.82/imgui_demo.cpp.o

.PHONY : Headers/imgui-1.82/imgui_demo.o

# target to build an object file
Headers/imgui-1.82/imgui_demo.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui_demo.cpp.o
.PHONY : Headers/imgui-1.82/imgui_demo.cpp.o

Headers/imgui-1.82/imgui_demo.i: Headers/imgui-1.82/imgui_demo.cpp.i

.PHONY : Headers/imgui-1.82/imgui_demo.i

# target to preprocess a source file
Headers/imgui-1.82/imgui_demo.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui_demo.cpp.i
.PHONY : Headers/imgui-1.82/imgui_demo.cpp.i

Headers/imgui-1.82/imgui_demo.s: Headers/imgui-1.82/imgui_demo.cpp.s

.PHONY : Headers/imgui-1.82/imgui_demo.s

# target to generate assembly for a file
Headers/imgui-1.82/imgui_demo.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui_demo.cpp.s
.PHONY : Headers/imgui-1.82/imgui_demo.cpp.s

Headers/imgui-1.82/imgui_draw.o: Headers/imgui-1.82/imgui_draw.cpp.o

.PHONY : Headers/imgui-1.82/imgui_draw.o

# target to build an object file
Headers/imgui-1.82/imgui_draw.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui_draw.cpp.o
.PHONY : Headers/imgui-1.82/imgui_draw.cpp.o

Headers/imgui-1.82/imgui_draw.i: Headers/imgui-1.82/imgui_draw.cpp.i

.PHONY : Headers/imgui-1.82/imgui_draw.i

# target to preprocess a source file
Headers/imgui-1.82/imgui_draw.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui_draw.cpp.i
.PHONY : Headers/imgui-1.82/imgui_draw.cpp.i

Headers/imgui-1.82/imgui_draw.s: Headers/imgui-1.82/imgui_draw.cpp.s

.PHONY : Headers/imgui-1.82/imgui_draw.s

# target to generate assembly for a file
Headers/imgui-1.82/imgui_draw.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui_draw.cpp.s
.PHONY : Headers/imgui-1.82/imgui_draw.cpp.s

Headers/imgui-1.82/imgui_tables.o: Headers/imgui-1.82/imgui_tables.cpp.o

.PHONY : Headers/imgui-1.82/imgui_tables.o

# target to build an object file
Headers/imgui-1.82/imgui_tables.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui_tables.cpp.o
.PHONY : Headers/imgui-1.82/imgui_tables.cpp.o

Headers/imgui-1.82/imgui_tables.i: Headers/imgui-1.82/imgui_tables.cpp.i

.PHONY : Headers/imgui-1.82/imgui_tables.i

# target to preprocess a source file
Headers/imgui-1.82/imgui_tables.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui_tables.cpp.i
.PHONY : Headers/imgui-1.82/imgui_tables.cpp.i

Headers/imgui-1.82/imgui_tables.s: Headers/imgui-1.82/imgui_tables.cpp.s

.PHONY : Headers/imgui-1.82/imgui_tables.s

# target to generate assembly for a file
Headers/imgui-1.82/imgui_tables.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui_tables.cpp.s
.PHONY : Headers/imgui-1.82/imgui_tables.cpp.s

Headers/imgui-1.82/imgui_widgets.o: Headers/imgui-1.82/imgui_widgets.cpp.o

.PHONY : Headers/imgui-1.82/imgui_widgets.o

# target to build an object file
Headers/imgui-1.82/imgui_widgets.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui_widgets.cpp.o
.PHONY : Headers/imgui-1.82/imgui_widgets.cpp.o

Headers/imgui-1.82/imgui_widgets.i: Headers/imgui-1.82/imgui_widgets.cpp.i

.PHONY : Headers/imgui-1.82/imgui_widgets.i

# target to preprocess a source file
Headers/imgui-1.82/imgui_widgets.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui_widgets.cpp.i
.PHONY : Headers/imgui-1.82/imgui_widgets.cpp.i

Headers/imgui-1.82/imgui_widgets.s: Headers/imgui-1.82/imgui_widgets.cpp.s

.PHONY : Headers/imgui-1.82/imgui_widgets.s

# target to generate assembly for a file
Headers/imgui-1.82/imgui_widgets.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/Headers/imgui-1.82/imgui_widgets.cpp.s
.PHONY : Headers/imgui-1.82/imgui_widgets.cpp.s

main.o: main.cpp.o

.PHONY : main.o

# target to build an object file
main.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/main.cpp.o
.PHONY : main.cpp.o

main.i: main.cpp.i

.PHONY : main.i

# target to preprocess a source file
main.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/main.cpp.i
.PHONY : main.cpp.i

main.s: main.cpp.s

.PHONY : main.s

# target to generate assembly for a file
main.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/main.cpp.s
.PHONY : main.cpp.s

mvAllocator.o: mvAllocator.cpp.o

.PHONY : mvAllocator.o

# target to build an object file
mvAllocator.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvAllocator.cpp.o
.PHONY : mvAllocator.cpp.o

mvAllocator.i: mvAllocator.cpp.i

.PHONY : mvAllocator.i

# target to preprocess a source file
mvAllocator.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvAllocator.cpp.i
.PHONY : mvAllocator.cpp.i

mvAllocator.s: mvAllocator.cpp.s

.PHONY : mvAllocator.s

# target to generate assembly for a file
mvAllocator.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvAllocator.cpp.s
.PHONY : mvAllocator.cpp.s

mvBuffer.o: mvBuffer.cpp.o

.PHONY : mvBuffer.o

# target to build an object file
mvBuffer.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvBuffer.cpp.o
.PHONY : mvBuffer.cpp.o

mvBuffer.i: mvBuffer.cpp.i

.PHONY : mvBuffer.i

# target to preprocess a source file
mvBuffer.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvBuffer.cpp.i
.PHONY : mvBuffer.cpp.i

mvBuffer.s: mvBuffer.cpp.s

.PHONY : mvBuffer.s

# target to generate assembly for a file
mvBuffer.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvBuffer.cpp.s
.PHONY : mvBuffer.cpp.s

mvCamera.o: mvCamera.cpp.o

.PHONY : mvCamera.o

# target to build an object file
mvCamera.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvCamera.cpp.o
.PHONY : mvCamera.cpp.o

mvCamera.i: mvCamera.cpp.i

.PHONY : mvCamera.i

# target to preprocess a source file
mvCamera.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvCamera.cpp.i
.PHONY : mvCamera.cpp.i

mvCamera.s: mvCamera.cpp.s

.PHONY : mvCamera.s

# target to generate assembly for a file
mvCamera.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvCamera.cpp.s
.PHONY : mvCamera.cpp.s

mvCollection.o: mvCollection.cpp.o

.PHONY : mvCollection.o

# target to build an object file
mvCollection.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvCollection.cpp.o
.PHONY : mvCollection.cpp.o

mvCollection.i: mvCollection.cpp.i

.PHONY : mvCollection.i

# target to preprocess a source file
mvCollection.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvCollection.cpp.i
.PHONY : mvCollection.cpp.i

mvCollection.s: mvCollection.cpp.s

.PHONY : mvCollection.s

# target to generate assembly for a file
mvCollection.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvCollection.cpp.s
.PHONY : mvCollection.cpp.s

mvEngine.o: mvEngine.cpp.o

.PHONY : mvEngine.o

# target to build an object file
mvEngine.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvEngine.cpp.o
.PHONY : mvEngine.cpp.o

mvEngine.i: mvEngine.cpp.i

.PHONY : mvEngine.i

# target to preprocess a source file
mvEngine.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvEngine.cpp.i
.PHONY : mvEngine.cpp.i

mvEngine.s: mvEngine.cpp.s

.PHONY : mvEngine.s

# target to generate assembly for a file
mvEngine.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvEngine.cpp.s
.PHONY : mvEngine.cpp.s

mvGui.o: mvGui.cpp.o

.PHONY : mvGui.o

# target to build an object file
mvGui.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvGui.cpp.o
.PHONY : mvGui.cpp.o

mvGui.i: mvGui.cpp.i

.PHONY : mvGui.i

# target to preprocess a source file
mvGui.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvGui.cpp.i
.PHONY : mvGui.cpp.i

mvGui.s: mvGui.cpp.s

.PHONY : mvGui.s

# target to generate assembly for a file
mvGui.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvGui.cpp.s
.PHONY : mvGui.cpp.s

mvHelper.o: mvHelper.cpp.o

.PHONY : mvHelper.o

# target to build an object file
mvHelper.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvHelper.cpp.o
.PHONY : mvHelper.cpp.o

mvHelper.i: mvHelper.cpp.i

.PHONY : mvHelper.i

# target to preprocess a source file
mvHelper.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvHelper.cpp.i
.PHONY : mvHelper.cpp.i

mvHelper.s: mvHelper.cpp.s

.PHONY : mvHelper.s

# target to generate assembly for a file
mvHelper.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvHelper.cpp.s
.PHONY : mvHelper.cpp.s

mvImage.o: mvImage.cpp.o

.PHONY : mvImage.o

# target to build an object file
mvImage.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvImage.cpp.o
.PHONY : mvImage.cpp.o

mvImage.i: mvImage.cpp.i

.PHONY : mvImage.i

# target to preprocess a source file
mvImage.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvImage.cpp.i
.PHONY : mvImage.cpp.i

mvImage.s: mvImage.cpp.s

.PHONY : mvImage.s

# target to generate assembly for a file
mvImage.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvImage.cpp.s
.PHONY : mvImage.cpp.s

mvInput.o: mvInput.cpp.o

.PHONY : mvInput.o

# target to build an object file
mvInput.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvInput.cpp.o
.PHONY : mvInput.cpp.o

mvInput.i: mvInput.cpp.i

.PHONY : mvInput.i

# target to preprocess a source file
mvInput.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvInput.cpp.i
.PHONY : mvInput.cpp.i

mvInput.s: mvInput.cpp.s

.PHONY : mvInput.s

# target to generate assembly for a file
mvInput.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvInput.cpp.s
.PHONY : mvInput.cpp.s

mvMap.o: mvMap.cpp.o

.PHONY : mvMap.o

# target to build an object file
mvMap.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvMap.cpp.o
.PHONY : mvMap.cpp.o

mvMap.i: mvMap.cpp.i

.PHONY : mvMap.i

# target to preprocess a source file
mvMap.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvMap.cpp.i
.PHONY : mvMap.cpp.i

mvMap.s: mvMap.cpp.s

.PHONY : mvMap.s

# target to generate assembly for a file
mvMap.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvMap.cpp.s
.PHONY : mvMap.cpp.s

mvModel.o: mvModel.cpp.o

.PHONY : mvModel.o

# target to build an object file
mvModel.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvModel.cpp.o
.PHONY : mvModel.cpp.o

mvModel.i: mvModel.cpp.i

.PHONY : mvModel.i

# target to preprocess a source file
mvModel.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvModel.cpp.i
.PHONY : mvModel.cpp.i

mvModel.s: mvModel.cpp.s

.PHONY : mvModel.s

# target to generate assembly for a file
mvModel.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvModel.cpp.s
.PHONY : mvModel.cpp.s

mvSwap.o: mvSwap.cpp.o

.PHONY : mvSwap.o

# target to build an object file
mvSwap.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvSwap.cpp.o
.PHONY : mvSwap.cpp.o

mvSwap.i: mvSwap.cpp.i

.PHONY : mvSwap.i

# target to preprocess a source file
mvSwap.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvSwap.cpp.i
.PHONY : mvSwap.cpp.i

mvSwap.s: mvSwap.cpp.s

.PHONY : mvSwap.s

# target to generate assembly for a file
mvSwap.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvSwap.cpp.s
.PHONY : mvSwap.cpp.s

mvTimer.o: mvTimer.cpp.o

.PHONY : mvTimer.o

# target to build an object file
mvTimer.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvTimer.cpp.o
.PHONY : mvTimer.cpp.o

mvTimer.i: mvTimer.cpp.i

.PHONY : mvTimer.i

# target to preprocess a source file
mvTimer.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvTimer.cpp.i
.PHONY : mvTimer.cpp.i

mvTimer.s: mvTimer.cpp.s

.PHONY : mvTimer.s

# target to generate assembly for a file
mvTimer.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvTimer.cpp.s
.PHONY : mvTimer.cpp.s

mvWindow.o: mvWindow.cpp.o

.PHONY : mvWindow.o

# target to build an object file
mvWindow.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvWindow.cpp.o
.PHONY : mvWindow.cpp.o

mvWindow.i: mvWindow.cpp.i

.PHONY : mvWindow.i

# target to preprocess a source file
mvWindow.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvWindow.cpp.i
.PHONY : mvWindow.cpp.i

mvWindow.s: mvWindow.cpp.s

.PHONY : mvWindow.s

# target to generate assembly for a file
mvWindow.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/mvWindow.cpp.s
.PHONY : mvWindow.cpp.s

# Help Target
help:
	@echo "The following are some of the valid targets for this Makefile:"
	@echo "... all (the default if no target is provided)"
	@echo "... clean"
	@echo "... depend"
	@echo "... edit_cache"
	@echo "... rebuild_cache"
	@echo "... main"
	@echo "... Headers/imgui-1.82/backends/imgui_impl_glfw.o"
	@echo "... Headers/imgui-1.82/backends/imgui_impl_glfw.i"
	@echo "... Headers/imgui-1.82/backends/imgui_impl_glfw.s"
	@echo "... Headers/imgui-1.82/backends/imgui_impl_vulkan.o"
	@echo "... Headers/imgui-1.82/backends/imgui_impl_vulkan.i"
	@echo "... Headers/imgui-1.82/backends/imgui_impl_vulkan.s"
	@echo "... Headers/imgui-1.82/imgui.o"
	@echo "... Headers/imgui-1.82/imgui.i"
	@echo "... Headers/imgui-1.82/imgui.s"
	@echo "... Headers/imgui-1.82/imgui_demo.o"
	@echo "... Headers/imgui-1.82/imgui_demo.i"
	@echo "... Headers/imgui-1.82/imgui_demo.s"
	@echo "... Headers/imgui-1.82/imgui_draw.o"
	@echo "... Headers/imgui-1.82/imgui_draw.i"
	@echo "... Headers/imgui-1.82/imgui_draw.s"
	@echo "... Headers/imgui-1.82/imgui_tables.o"
	@echo "... Headers/imgui-1.82/imgui_tables.i"
	@echo "... Headers/imgui-1.82/imgui_tables.s"
	@echo "... Headers/imgui-1.82/imgui_widgets.o"
	@echo "... Headers/imgui-1.82/imgui_widgets.i"
	@echo "... Headers/imgui-1.82/imgui_widgets.s"
	@echo "... main.o"
	@echo "... main.i"
	@echo "... main.s"
	@echo "... mvAllocator.o"
	@echo "... mvAllocator.i"
	@echo "... mvAllocator.s"
	@echo "... mvBuffer.o"
	@echo "... mvBuffer.i"
	@echo "... mvBuffer.s"
	@echo "... mvCamera.o"
	@echo "... mvCamera.i"
	@echo "... mvCamera.s"
	@echo "... mvCollection.o"
	@echo "... mvCollection.i"
	@echo "... mvCollection.s"
	@echo "... mvEngine.o"
	@echo "... mvEngine.i"
	@echo "... mvEngine.s"
	@echo "... mvGui.o"
	@echo "... mvGui.i"
	@echo "... mvGui.s"
	@echo "... mvHelper.o"
	@echo "... mvHelper.i"
	@echo "... mvHelper.s"
	@echo "... mvImage.o"
	@echo "... mvImage.i"
	@echo "... mvImage.s"
	@echo "... mvInput.o"
	@echo "... mvInput.i"
	@echo "... mvInput.s"
	@echo "... mvMap.o"
	@echo "... mvMap.i"
	@echo "... mvMap.s"
	@echo "... mvModel.o"
	@echo "... mvModel.i"
	@echo "... mvModel.s"
	@echo "... mvSwap.o"
	@echo "... mvSwap.i"
	@echo "... mvSwap.s"
	@echo "... mvTimer.o"
	@echo "... mvTimer.i"
	@echo "... mvTimer.s"
	@echo "... mvWindow.o"
	@echo "... mvWindow.i"
	@echo "... mvWindow.s"
.PHONY : help



#=============================================================================
# Special targets to cleanup operation of make.

# Special rule to run CMake to check the build system integrity.
# No rule that depends on this can have commands that come from listfiles
# because they might be regenerated.
cmake_check_build_system:
	$(CMAKE_COMMAND) -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 0
.PHONY : cmake_check_build_system

