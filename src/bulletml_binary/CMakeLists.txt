cmake_minimum_required(VERSION 3.10)
project(BulletMLBinary)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Access SaturnRingLib headers from subdirectory builds
include_directories(/saturn/SaturnRingLib/saturnringlib)

# BulletML Binary Library (header-only)
add_library(bulletml_binary INTERFACE)
target_include_directories(bulletml_binary INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# No external dependencies needed for Saturn port (header-only)

# Install targets
install(TARGETS bulletml_binary
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    bulletmlparser_blb.hpp
    BINARY_FORMAT.md
    DESTINATION include/bulletml_binary
)
