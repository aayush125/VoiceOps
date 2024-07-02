cmake_minimum_required(VERSION 3.20)
project("VoiceOps" CXX)
set(CMAKE_CXX_STANDARD 17)

# Generate PDB when using llvm-mingw
if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-g -gcodeview)
    add_link_options("-Wl,--pdb=")
endif()

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# Common config for both server and client
include_directories(headers)
link_directories(lib)
link_libraries(sqlite3 opus)

# WinSock for both server and client
if (WIN32)
    link_libraries(ws2_32)
endif()

# VoiceOps Client - Windows only
if (WIN32)
    add_executable(client src/main.cpp src/window.cpp src/voice.cpp src/screenshot.cpp src/networking.cpp src/lodepng.cpp src/dialogs.cpp src/database.cpp src/common/jitter_buffer.cpp src/common/text_packet.cpp)
    target_include_directories(client PRIVATE ${GTKMM_INCLUDE_DIRS})
    target_link_directories(client PRIVATE ${GTKMM_LIBRARY_DIRS})
    target_link_libraries(client PRIVATE ${GTKMM_LIBRARIES} gdi32)
endif()

# VoiceOps Server
add_executable(server src/server/server.cpp src/server/voice_server.cpp src/common/text_packet.cpp src/common/jitter_buffer.cpp)
target_include_directories(server PRIVATE ${GLIB_INCLUDE_DIRS})
target_link_directories(server PRIVATE ${GLIB_LIBRARY_DIRS})
target_link_libraries(server PRIVATE ${GLIB_LIBRARIES} renamenoise)

if (WIN32)
    target_link_libraries(server PRIVATE winmm)
endif()
