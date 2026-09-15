cmake_minimum_required(VERSION 3.15)
project(compiler_ad_member3 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Member 3 module library: Reverse AD implementation
# ---------------------------------------------------------------------------
add_library(compiler_ad_member3 STATIC
    src/reverse_ad/ReverseAD.cpp
)

target_include_directories(compiler_ad_member3 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link against Member 4's core library (for IR, Validator, etc.)
target_link_libraries(compiler_ad_member3 PUBLIC compiler_ad_core)

# ---------------------------------------------------------------------------
# Standalone test harness executable (Member 3's own module tests).
# ---------------------------------------------------------------------------
add_executable(member3_tests
    tests/main.cpp
)

target_link_libraries(member3_tests PRIVATE compiler_ad_member3 compiler_ad_core)

enable_testing()
add_test(NAME member3_module_tests COMMAND member3_tests)
