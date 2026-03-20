cmake_minimum_required(VERSION 3.16)
project(malloc_new_test LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MALLOC_NEW_TEST_ENABLE_LTO "Enable link-time optimization" OFF)
set(MALLOC_NEW_GIT_REPOSITORY "https://github.com/Jerryy959/malloc_new.git" CACHE STRING
    "Git repository used to fetch malloc_new when MALLOC_NEW_SOURCE_DIR is not provided")
set(MALLOC_NEW_GIT_TAG "main" CACHE STRING
    "Git tag/branch/commit used when fetching malloc_new")
set(MALLOC_NEW_SOURCE_DIR "" CACHE PATH
    "Optional local path to malloc_new sources; if empty, CMake clones from MALLOC_NEW_GIT_REPOSITORY")

if(MALLOC_NEW_TEST_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
  if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  else()
    message(WARNING "IPO/LTO not supported: ${ipo_error}")
  endif()
endif()

if(MALLOC_NEW_SOURCE_DIR)
  set(malloc_new_SOURCE_DIR "${MALLOC_NEW_SOURCE_DIR}")
  message(STATUS "Using local malloc_new source directory: ${malloc_new_SOURCE_DIR}")
else()
  message(STATUS "Fetching malloc_new from ${MALLOC_NEW_GIT_REPOSITORY} (${MALLOC_NEW_GIT_TAG})")
  FetchContent_Declare(
    malloc_new
    GIT_REPOSITORY ${MALLOC_NEW_GIT_REPOSITORY}
    GIT_TAG ${MALLOC_NEW_GIT_TAG}
    GIT_SHALLOW TRUE
  )
  FetchContent_GetProperties(malloc_new)
  if(NOT malloc_new_POPULATED)
    FetchContent_Populate(malloc_new)
  endif()
endif()

if(EXISTS "${malloc_new_SOURCE_DIR}/CMakeLists.txt")
  add_subdirectory("${malloc_new_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/malloc_new-build")
else()
  add_library(malloc_new STATIC
    "${malloc_new_SOURCE_DIR}/src/huge_page_allocator.cpp"
    "${malloc_new_SOURCE_DIR}/src/compat.cpp"
  )
  target_include_directories(malloc_new PUBLIC "${malloc_new_SOURCE_DIR}/include")
  find_package(Threads REQUIRED)
  target_link_libraries(malloc_new PUBLIC Threads::Threads)
  target_compile_features(malloc_new PUBLIC cxx_std_20)
endif()

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
