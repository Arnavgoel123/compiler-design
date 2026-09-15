cmake_minimum_required(VERSION 3.14)
project(compiler_ad_member2 CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(ForwardAD ForwardAD.cpp ForwardAD.h)
target_include_directories(ForwardAD PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ForwardAD PUBLIC IRCore)

add_executable(forward_ad_test ForwardADTest.cpp)
target_link_libraries(forward_ad_test PRIVATE ForwardAD IRCore)

feat: add CMakeLists.txt
