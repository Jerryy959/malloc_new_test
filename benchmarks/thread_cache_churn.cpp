#include "benchmark_common.hpp"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
  auto cfg = bench::parse_args(argc, argv);
  bench::print_config(cfg, "thread_cache_churn");

  std::vector<std::thread> workers;
  std::vector<double> thread_samples(cfg.threads, 0.0);
  std::atomic<std::size_t> ready{0};
  std::atomic<bool> start{false};

  for (std::size_t t = 0; t < cfg.threads; ++t) {
    workers.emplace_back([&, t] {
      bench::pin_thread_if_requested(t);
      std::uint64_t rng = cfg.seed + t * 101;
      std::vector<void*> batch(cfg.batch, nullptr);
      ready.fetch_add(1, std::memory_order_relaxed);
      while (!start.load(std::memory_order_acquire)) {}

      thread_samples[t] = bench::measure_ns([&] {
        for (std::size_t i = 0; i < cfg.iterations; ++i) {
          for (std::size_t j = 0; j < cfg.batch; ++j) {
            std::size_t size = cfg.min_size + (bench::fast_random(rng) % (cfg.max_size - cfg.min_size + 1));
            batch[j] = bench::alloc(size);
            if (cfg.touch_memory) bench::touch_memory(batch[j], size);
          }
          for (void* ptr : batch) {
            bench::free(ptr);
          }
        }
      });
    });
  }

  while (ready.load(std::memory_order_acquire) != cfg.threads) {}
  start.store(true, std::memory_order_release);

  for (auto& worker : workers) worker.join();
  bench::print_latency_summary("thread_cache_churn_total", thread_samples, cfg.iterations * cfg.batch * cfg.threads);
  bench::print_allocator_stats();
  return 0;
}
