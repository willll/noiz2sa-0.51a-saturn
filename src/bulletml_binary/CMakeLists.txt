cmake_minimum_required(VERSION 3.10)
project(BulletMLBinary)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# BulletML Binary Library
add_library(bulletml_binary STATIC
    bulletmlparser-bin.cpp
)

# No external dependencies needed for Saturn port (header-only)

# Install targets
install(TARGETS bulletml_binary
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    bulletmlparser_binary.hpp
    BINARY_FORMAT.md
    DESTINATION include/bulletml_binary
)
