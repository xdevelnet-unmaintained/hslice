cmake_minimum_required(VERSION 3.2)
project(hslice C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

set(SOURCE_FILES
    main.c libhslice.c tests.h libhslice_real.h examples/lorem.c)

add_executable(hslice_tests ${SOURCE_FILES})
