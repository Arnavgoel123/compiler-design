cmake_minimum_required(VERSION 3.15)
project(compiler_ad_member1 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(compiler_ad_frontend STATIC
    src/Lexer.cpp
    src/Parser.cpp
    src/SemanticAnalyzer.cpp
    src/Frontend.cpp
    src/ASTPrinter.cpp
)

target_include_directories(compiler_ad_frontend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(member1_tests tests/main.cpp)
target_link_libraries(member1_tests PRIVATE compiler_ad_frontend)

enable_testing()
add_test(NAME member1_frontend_tests COMMAND member1_tests)
