#include "benchmark_common.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
  auto cfg = bench::parse_args(argc, argv);
  bench::print_config(cfg, "realloc_growth");

  std::vector<double> samples;
  samples.reserve(cfg.iterations);

  for (std::size_t i = 0; i < cfg.iterations; ++i) {
    void* ptr = bench::alloc(cfg.min_size);
    if (cfg.touch_memory) bench::touch_memory(ptr, cfg.min_size);
    std::size_t size = cfg.min_size;
    samples.push_back(bench::measure_ns([&] {
      for (std::size_t step = 0; step < cfg.realloc_steps; ++step) {
        size = std::min(cfg.max_size, size * 2);
        ptr = bench::realloc(ptr, size);
        if (cfg.touch_memory) bench::touch_memory(ptr, size);
      }
      bench::free(ptr);
    }));
  }

  bench::print_latency_summary("realloc_growth_cycle", samples, cfg.iterations * cfg.realloc_steps);
  bench::print_allocator_stats();
  return 0;
}
