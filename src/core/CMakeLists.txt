cmake_minimum_required(VERSION 3.0.0)
project(core VERSION 0.1.0)
include(CTest)

add_compile_options(-Werror -Wall -Wno-missing-braces -O3)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
    add_compile_options(-march=native)
    message(STATUS "-march=native is enabled for core")
endif()
if(ENABLE_FFAST_MATH)
    add_compile_options(-ffast-math)
    message(STATUS "-ffast-math is enabled for core")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB core_lib_SRC
     "*.cpp"
)
add_library(core ${core_lib_SRC})
target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR})

add_subdirectory(tests)