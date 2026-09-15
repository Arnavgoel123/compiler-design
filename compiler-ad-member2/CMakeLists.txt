cmake_minimum_required(VERSION 3.15)
project(compiler_ad_member2 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
	add_compile_options(/W4)
else()
	add_compile_options(-Wall -Wextra)
endif()

add_library(compiler_ad_member2 STATIC
	src/ForwardAD.cpp
)

target_include_directories(compiler_ad_member2 PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(compiler_ad_member2 PUBLIC compiler_ad_core)

add_executable(member2_tests
	tests/main.cpp
)

target_link_libraries(member2_tests PRIVATE compiler_ad_member2 compiler_ad_core)

enable_testing()
add_test(NAME member2_module_tests COMMAND member2_tests)
