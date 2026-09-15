cmake_minimum_required(VERSION 3.15)
project(compiler_ad CXX)

# Include both member modules so the whole project can be built together.
add_subdirectory(compiler-ad-member4)
add_subdirectory(compiler-ad-member3)
add_subdirectory(compiler-ad-member2)
