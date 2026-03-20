#include "benchmark_common.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>

int main(int argc, char** argv) {
  auto cfg = bench::parse_args(argc, argv);
  bench::print_config(cfg, "fragmentation_reuse");

  std::vector<void*> ptrs(cfg.objects, nullptr);
  std::vector<std::size_t> sizes(cfg.objects, cfg.min_size);
  std::uint64_t rng = cfg.seed;

  for (std::size_t i = 0; i < cfg.objects; ++i) {
    sizes[i] = cfg.min_size + (bench::fast_random(rng) % (cfg.max_size - cfg.min_size + 1));
    ptrs[i] = bench::alloc(sizes[i]);
    if (cfg.touch_memory) bench::touch_memory(ptrs[i], sizes[i]);
  }

  std::vector<std::size_t> order(cfg.objects);
  for (std::size_t i = 0; i < cfg.objects; ++i) order[i] = i;
  if (cfg.random_order) {
    std::shuffle(order.begin(), order.end(), std::default_random_engine(static_cast<unsigned>(cfg.seed)));
  }

  for (std::size_t i = 0; i < cfg.objects; i += 2) {
    bench::free(ptrs[order[i]]);
    ptrs[order[i]] = nullptr;
  }

  std::vector<double> samples;
  samples.reserve(cfg.iterations);
  for (std::size_t i = 0; i < cfg.iterations; ++i) {
    const std::size_t idx = order[(i * 2) % cfg.objects];
    const std::size_t size = cfg.min_size + (bench::fast_random(rng) % (cfg.max_size - cfg.min_size + 1));
    samples.push_back(bench::measure_ns([&] {
      ptrs[idx] = bench::alloc(size);
      if (cfg.touch_memory) bench::touch_memory(ptrs[idx], size);
      bench::free(ptrs[idx]);
      ptrs[idx] = nullptr;
    }));
  }

  for (void* ptr : ptrs) {
    bench::free(ptr);
  }

  bench::print_latency_summary("fragmentation_reuse_cycle", samples, cfg.iterations);
  bench::print_allocator_stats();
  return 0;
}
