cmake_minimum_required(VERSION 3.16)
project(malloc_new_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MALLOC_NEW_TEST_ENABLE_LTO "Enable link-time optimization" OFF)

if(MALLOC_NEW_TEST_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
  if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  else()
    message(WARNING "IPO/LTO not supported: ${ipo_error}")
  endif()
endif()

add_subdirectory(third_party/malloc_new)

add_library(benchmark_common STATIC
  src/benchmark_common.cpp
)

target_include_directories(benchmark_common PUBLIC include)
target_link_libraries(benchmark_common PUBLIC malloc_new)

target_compile_options(benchmark_common PRIVATE
  -Wall -Wextra -Wpedantic
)

function(add_malloc_benchmark target)
  add_executable(${target} benchmarks/${target}.cpp)
  target_link_libraries(${target} PRIVATE benchmark_common malloc_new pthread)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endfunction()

add_malloc_benchmark(latency_fixed)
add_malloc_benchmark(latency_size_sweep)
add_malloc_benchmark(thread_cache_churn)
add_malloc_benchmark(remote_free)
add_malloc_benchmark(realloc_growth)
add_malloc_benchmark(large_alloc)
add_malloc_benchmark(fragmentation_reuse)
add_malloc_benchmark(burst_contention)
add_malloc_benchmark(new_delete_mix)
