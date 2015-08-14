cmake_minimum_required(VERSION 3.2)
project (Packages)
add_executable (Packages Packages.c)
set_property(TARGET Packages PROPERTY C_STANDARD 99)

set(FUSE_COMPILE_FLAGS "-D_FILE_OFFSET_BITS=64")
add_definitions(${FUSE_COMPILE_FLAGS})

include(FindPkgConfig)
pkg_search_module(FUSE REQUIRED fuse)
target_link_libraries(Packages "${FUSE_LIBRARIES}")

