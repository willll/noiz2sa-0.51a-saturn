cmake_minimum_required(VERSION 3.0)

SET(CMAKE_SYSTEM_NAME Generic)

find_package(BISON)

set(DESTINATION_LIB lib)

project(bulletml
        VERSION 0.1
        DESCRIPTION "bulletml"
        LANGUAGES CXX)

set(cxx_flags -Wall
            -m2
            -O2 -g3
            -finline-functions
            -fomit-frame-pointer
            #-fno-rtti
            #-fno-exceptions
            -fno-lto
            )

add_compile_options(  "$<$<COMPILE_LANGUAGE:CXX>:${cxx_flags}>"
)

set(SOURCES
    bulletmlparser-tinyxml.cpp
    bulletmlparser.cpp
    bulletmltree.cpp
    calc.cpp
    formula-variables.cpp
    bulletmlrunner.cpp
    bulletmlrunnerimpl.cpp
    )

BISON_TARGET(MyParser calc.yy ${PROJECT_SOURCE_DIR}/calc.cpp COMPILE_FLAGS -l)

add_compile_definitions(_SH MODEL_S)

add_library( bulletml STATIC
             ${SOURCES} ${BISON_MyParser_OUTPUTS} )

SET_TARGET_PROPERTIES(bulletml PROPERTIES PREFIX "")

install(TARGETS bulletml DESTINATION ${PROJECT_SOURCE_DIR}/lib)
