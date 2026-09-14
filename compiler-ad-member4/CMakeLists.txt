cmake_minimum_required(VERSION 3.15)
project(compiler_ad_member4 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Member 4 module library: IR, AST->IR, Optimizer, CodeGenerator, Validation.
# ---------------------------------------------------------------------------
add_library(compiler_ad_core STATIC
    src/ir/IR.cpp
    src/ir/ASTToIR.cpp
    src/optimizer/Optimizer.cpp
    src/codegen/CodeGenerator.cpp
    src/validation/Validator.cpp
)

target_include_directories(compiler_ad_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ---------------------------------------------------------------------------
# Standalone test harness executable (Member 4's own module tests).
# ---------------------------------------------------------------------------
add_executable(member4_tests
    tests/main.cpp
)

target_link_libraries(member4_tests PRIVATE compiler_ad_core)

enable_testing()
add_test(NAME member4_module_tests COMMAND member4_tests)
