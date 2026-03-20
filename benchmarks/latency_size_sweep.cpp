#include "benchmark_common.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
  auto cfg = bench::parse_args(argc, argv);
  bench::print_config(cfg, "latency_size_sweep");
  auto sizes = bench::build_size_pattern(cfg);

  for (std::size_t size : sizes) {
    std::vector<double> samples;
    samples.reserve(cfg.iterations);
    for (std::size_t i = 0; i < cfg.warmup; ++i) {
      void* ptr = bench::alloc(size);
      if (cfg.touch_memory) bench::touch_memory(ptr, size);
      bench::free(ptr);
    }
    for (std::size_t i = 0; i < cfg.iterations; ++i) {
      samples.push_back(bench::measure_ns([&] {
        void* ptr = bench::alloc(size);
        if (cfg.touch_memory) bench::touch_memory(ptr, size);
        bench::free(ptr);
      }));
    }
    bench::print_latency_summary("malloc_free_size_" + std::to_string(size), samples, cfg.iterations);
  }
  bench::print_allocator_stats();
  return 0;
}
