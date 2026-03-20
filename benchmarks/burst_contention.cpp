#include "benchmark_common.hpp"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
  auto cfg = bench::parse_args(argc, argv);
  bench::print_config(cfg, "burst_contention");

  std::atomic<std::size_t> ready{0};
  std::atomic<bool> go{false};
  std::vector<std::thread> threads;
  std::vector<double> samples(cfg.threads, 0.0);

  for (std::size_t t = 0; t < cfg.threads; ++t) {
    threads.emplace_back([&, t] {
      bench::pin_thread_if_requested(t);
      std::vector<void*> ptrs(cfg.batch, nullptr);
      ready.fetch_add(1, std::memory_order_release);
      while (!go.load(std::memory_order_acquire)) {}
      samples[t] = bench::measure_ns([&] {
        for (std::size_t i = 0; i < cfg.iterations; ++i) {
          for (std::size_t j = 0; j < cfg.batch; ++j) {
            ptrs[j] = bench::alloc(cfg.min_size);
          }
          for (void* ptr : ptrs) {
            bench::free(ptr);
          }
        }
      });
    });
  }

  while (ready.load(std::memory_order_acquire) != cfg.threads) {}
  go.store(true, std::memory_order_release);
  for (auto& t : threads) t.join();

  bench::print_latency_summary("burst_contention_total", samples, cfg.iterations * cfg.batch * cfg.threads);
  bench::print_allocator_stats();
  return 0;
}
