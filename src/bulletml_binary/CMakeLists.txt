cmake_minimum_required(VERSION 3.10)
project(BulletMLBinary)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include parent bulletml directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# BulletML Binary Library
add_library(bulletml_binary STATIC
    bulletmlparser-bin.cpp
    bulletmlparser-bin.h
)

# Link with parent bulletml library
target_link_libraries(bulletml_binary
    ${CMAKE_SOURCE_DIR}/src/bulletml/libbulletml.a
)

# BulletML Converter Tool
add_executable(bulletml-converter
    bulletml-converter.cpp
)

target_link_libraries(bulletml-converter
    ${CMAKE_SOURCE_DIR}/src/bulletml/libbulletml.a
)

    # Test program
    add_executable(test-binary
        test-binary.cpp
    )

    target_link_libraries(test-binary
        bulletml_binary
        ${CMAKE_SOURCE_DIR}/src/bulletml/libbulletml.a
    )

# Install targets
install(TARGETS bulletml_binary bulletml-converter
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    bulletmlparser-bin.h
    BINARY_FORMAT.md
    DESTINATION include/bulletml_binary
)
