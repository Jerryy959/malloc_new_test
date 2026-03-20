#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "malloc_new/allocator.hpp"

namespace bench {

struct Config {
  std::size_t iterations = 1'000'000;
  std::size_t warmup = 100'000;
  std::size_t min_size = 16;
  std::size_t max_size = 256;
  std::size_t step = 16;
  std::size_t threads = 1;
  std::size_t batch = 64;
  std::size_t queue_depth = 4096;
  std::size_t objects = 4096;
  std::size_t realloc_steps = 8;
  std::size_t seed = 12345;
  bool touch_memory = true;
  bool random_order = false;
};

Config parse_args(int argc, char** argv);
void print_config(const Config& cfg, const std::string& benchmark_name);
void touch_memory(void* ptr, std::size_t size);
void pin_thread_if_requested(std::size_t index);
void print_latency_summary(const std::string& name,
                           const std::vector<double>& samples_ns,
                           std::size_t operations);
void print_allocator_stats();
std::uint64_t fast_random(std::uint64_t& state);
std::vector<std::size_t> build_size_pattern(const Config& cfg);

inline void* alloc(std::size_t size) noexcept { return malloc_new::compat::malloc(size); }
inline void* aligned_alloc(std::size_t alignment, std::size_t size) noexcept {
  return malloc_new::compat::aligned_alloc(alignment, size);
}
inline void* calloc(std::size_t count, std::size_t size) noexcept {
  return malloc_new::compat::calloc(count, size);
}
inline void* realloc(void* ptr, std::size_t size) noexcept {
  return malloc_new::compat::realloc(ptr, size);
}
inline void free(void* ptr) noexcept { malloc_new::compat::free(ptr); }

template <typename Fn>
double measure_ns(Fn&& fn) {
  auto start = std::chrono::steady_clock::now();
  fn();
  auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::nano>(end - start).count();
}

}  // namespace bench
