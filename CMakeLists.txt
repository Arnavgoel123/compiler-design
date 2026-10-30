cmake_minimum_required(VERSION 3.15)
project(compiler_ad CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# Include all 4 member submodules
add_subdirectory(compiler-ad-member1)
add_subdirectory(compiler-ad-member4)
add_subdirectory(compiler-ad-member3)
add_subdirectory(compiler-ad-member2)

# Unified End-to-End Compiler Application
add_executable(compiler_ad src/main.cpp)
target_include_directories(compiler_ad PRIVATE
    compiler-ad-member1/include
    compiler-ad-member4/src
    compiler-ad-member2/src
    compiler-ad-member3/src
)
target_link_libraries(compiler_ad PRIVATE
    compiler_ad_frontend
    compiler_ad_core
    compiler_ad_member2
    compiler_ad_member3
)

# ---------------------------------------------------------------------------
# End-to-end tests of the unified compiler (multi-equation programs)
# ---------------------------------------------------------------------------
foreach(example chain rosenbrock neuron single)
    add_test(NAME e2e_${example}
             COMMAND compiler_ad --file ${CMAKE_CURRENT_SOURCE_DIR}/examples/${example}.dsl)
endforeach()

add_test(NAME e2e_inline_multi_equation
         COMMAND compiler_ad "input x, z; a = x*z; y = sin(a) + x^2; w = exp(y)/(1+z^2);")

# Invalid programs must make the compiler exit non-zero.
foreach(bad "input x; y = x; y = x + 1;" "input x; y = y + 1;" "input x; y = b + x; b = x;")
    string(MAKE_C_IDENTIFIER "${bad}" bad_id)
    add_test(NAME e2e_rejects_${bad_id} COMMAND compiler_ad "${bad}")
    set_tests_properties(e2e_rejects_${bad_id} PROPERTIES WILL_FAIL TRUE)
endforeach()
