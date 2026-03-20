#include "benchmark_common.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
  auto cfg = bench::parse_args(argc, argv);
  bench::print_config(cfg, "latency_fixed");

  const std::size_t size = cfg.min_size;
  std::vector<double> alloc_samples;
  std::vector<double> free_samples;
  alloc_samples.reserve(cfg.iterations);
  free_samples.reserve(cfg.iterations);

  for (std::size_t i = 0; i < cfg.warmup; ++i) {
    void* ptr = bench::alloc(size);
    if (cfg.touch_memory) bench::touch_memory(ptr, size);
    bench::free(ptr);
  }

  for (std::size_t i = 0; i < cfg.iterations; ++i) {
    void* ptr = nullptr;
    alloc_samples.push_back(bench::measure_ns([&] {
      ptr = bench::alloc(size);
      if (cfg.touch_memory) bench::touch_memory(ptr, size);
    }));
    free_samples.push_back(bench::measure_ns([&] { bench::free(ptr); }));
  }

  bench::print_latency_summary("malloc", alloc_samples, cfg.iterations);
  bench::print_latency_summary("free", free_samples, cfg.iterations);
  bench::print_allocator_stats();
  return 0;
}
