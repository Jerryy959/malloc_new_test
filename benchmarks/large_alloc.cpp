#include "benchmark_common.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
  auto cfg = bench::parse_args(argc, argv);
  bench::print_config(cfg, "large_alloc");

  std::vector<double> alloc_samples;
  std::vector<double> free_samples;
  alloc_samples.reserve(cfg.iterations);
  free_samples.reserve(cfg.iterations);

  for (std::size_t i = 0; i < cfg.iterations; ++i) {
    void* ptr = nullptr;
    alloc_samples.push_back(bench::measure_ns([&] {
      ptr = bench::alloc(cfg.max_size);
      if (cfg.touch_memory) bench::touch_memory(ptr, cfg.max_size);
    }));
    free_samples.push_back(bench::measure_ns([&] { bench::free(ptr); }));
  }

  bench::print_latency_summary("large_malloc", alloc_samples, cfg.iterations);
  bench::print_latency_summary("large_free", free_samples, cfg.iterations);
  bench::print_allocator_stats();
  return 0;
}
