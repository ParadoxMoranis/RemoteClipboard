# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.31

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
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/build

# Include any dependencies generated for this target.
include CMakeFiles/RemoteClipboardServer.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/RemoteClipboardServer.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/RemoteClipboardServer.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/RemoteClipboardServer.dir/flags.make

CMakeFiles/RemoteClipboardServer.dir/codegen:
.PHONY : CMakeFiles/RemoteClipboardServer.dir/codegen

CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.o: CMakeFiles/RemoteClipboardServer.dir/flags.make
CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.o: /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/server_main.cpp
CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.o: CMakeFiles/RemoteClipboardServer.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.o -MF CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.o.d -o CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.o -c /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/server_main.cpp

CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/server_main.cpp > CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.i

CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/server_main.cpp -o CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.s

CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.o: CMakeFiles/RemoteClipboardServer.dir/flags.make
CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.o: /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/tcpserver.cpp
CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.o: CMakeFiles/RemoteClipboardServer.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.o -MF CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.o.d -o CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.o -c /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/tcpserver.cpp

CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/tcpserver.cpp > CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.i

CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/tcpserver.cpp -o CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.s

# Object files for target RemoteClipboardServer
RemoteClipboardServer_OBJECTS = \
"CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.o" \
"CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.o"

# External object files for target RemoteClipboardServer
RemoteClipboardServer_EXTERNAL_OBJECTS =

RemoteClipboardServer: CMakeFiles/RemoteClipboardServer.dir/server_main.cpp.o
RemoteClipboardServer: CMakeFiles/RemoteClipboardServer.dir/tcpserver.cpp.o
RemoteClipboardServer: CMakeFiles/RemoteClipboardServer.dir/build.make
RemoteClipboardServer: CMakeFiles/RemoteClipboardServer.dir/compiler_depend.ts
RemoteClipboardServer: CMakeFiles/RemoteClipboardServer.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable RemoteClipboardServer"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/RemoteClipboardServer.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/RemoteClipboardServer.dir/build: RemoteClipboardServer
.PHONY : CMakeFiles/RemoteClipboardServer.dir/build

CMakeFiles/RemoteClipboardServer.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/RemoteClipboardServer.dir/cmake_clean.cmake
.PHONY : CMakeFiles/RemoteClipboardServer.dir/clean

CMakeFiles/RemoteClipboardServer.dir/depend:
	cd /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/build /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/build /home/paradox/Projects/RemotePasteboard/RemoteClipboardServer-512MB/build/CMakeFiles/RemoteClipboardServer.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/RemoteClipboardServer.dir/depend

