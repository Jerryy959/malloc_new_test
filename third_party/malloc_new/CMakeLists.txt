cmake_minimum_required(VERSION 3.16)
project(malloc_new LANGUAGES CXX)

find_package(Threads REQUIRED)

add_library(malloc_new STATIC
  src/huge_page_allocator.cpp
  src/compat.cpp
)

target_include_directories(malloc_new PUBLIC include)
target_compile_features(malloc_new PUBLIC cxx_std_20)
target_link_libraries(malloc_new PUBLIC Threads::Threads)
