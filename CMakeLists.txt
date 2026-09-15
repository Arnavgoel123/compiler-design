cmake_minimum_required(VERSION 3.15)
project(compiler_ad CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
