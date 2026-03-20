#include "benchmark_common.hpp"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
  auto cfg = bench::parse_args(argc, argv);
  bench::print_config(cfg, "remote_free");

  const std::size_t depth = cfg.queue_depth;
  std::vector<void*> slots(depth, nullptr);
  std::atomic<std::size_t> published{0};
  std::atomic<std::size_t> consumed{0};
  std::atomic<bool> producer_done{false};
  std::vector<double> producer_samples;
  std::vector<double> consumer_samples;
  producer_samples.reserve(cfg.iterations);
  consumer_samples.reserve(cfg.iterations);

  std::thread producer([&] {
    bench::pin_thread_if_requested(0);
    for (std::size_t i = 0; i < cfg.iterations; ++i) {
      auto sample = bench::measure_ns([&] {
        while (i - consumed.load(std::memory_order_acquire) >= depth) {}
        void* ptr = bench::alloc(cfg.min_size);
        if (cfg.touch_memory) bench::touch_memory(ptr, cfg.min_size);
        slots[i % depth] = ptr;
        published.store(i + 1, std::memory_order_release);
      });
      producer_samples.push_back(sample);
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::thread consumer([&] {
    bench::pin_thread_if_requested(1);
    std::size_t local_consumed = 0;
    while (!producer_done.load(std::memory_order_acquire) ||
           local_consumed < published.load(std::memory_order_acquire)) {
      const std::size_t available = published.load(std::memory_order_acquire);
      if (local_consumed >= available) {
        continue;
      }
      void* ptr = slots[local_consumed % depth];
      consumer_samples.push_back(bench::measure_ns([&] { bench::free(ptr); }));
      slots[local_consumed % depth] = nullptr;
      ++local_consumed;
      consumed.store(local_consumed, std::memory_order_release);
    }
  });

  producer.join();
  consumer.join();

  bench::print_latency_summary("remote_malloc", producer_samples, cfg.iterations);
  bench::print_latency_summary("remote_free", consumer_samples, cfg.iterations);
  bench::print_allocator_stats();
  return 0;
}
