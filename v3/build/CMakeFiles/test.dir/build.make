# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.21

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

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
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/nowcoder/memory-pool/memoryPool/memory-pool/v3

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/nowcoder/memory-pool/memoryPool/memory-pool/v3/build

# Utility rule file for test.

# Include any custom commands dependencies for this target.
include CMakeFiles/test.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/test.dir/progress.make

CMakeFiles/test: unit_test
	./unit_test

test: CMakeFiles/test
test: CMakeFiles/test.dir/build.make
.PHONY : test

# Rule to build all files generated by this target.
CMakeFiles/test.dir/build: test
.PHONY : CMakeFiles/test.dir/build

CMakeFiles/test.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/test.dir/cmake_clean.cmake
.PHONY : CMakeFiles/test.dir/clean

CMakeFiles/test.dir/depend:
	cd /home/nowcoder/memory-pool/memoryPool/memory-pool/v3/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/nowcoder/memory-pool/memoryPool/memory-pool/v3 /home/nowcoder/memory-pool/memoryPool/memory-pool/v3 /home/nowcoder/memory-pool/memoryPool/memory-pool/v3/build /home/nowcoder/memory-pool/memoryPool/memory-pool/v3/build /home/nowcoder/memory-pool/memoryPool/memory-pool/v3/build/CMakeFiles/test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/test.dir/depend

