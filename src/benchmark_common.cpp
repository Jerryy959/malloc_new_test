#include "benchmark_common.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace bench {
namespace {

std::size_t parse_size(const char* text) {
  return static_cast<std::size_t>(std::strtoull(text, nullptr, 10));
}

}  // namespace

Config parse_args(int argc, char** argv) {
  Config cfg;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next_value = [&](const char* name) -> const char* {
      if (i + 1 >= argc) {
        throw std::runtime_error(std::string("missing value for ") + name);
      }
      return argv[++i];
    };

    if (arg == "--iterations") {
      cfg.iterations = parse_size(next_value("--iterations"));
    } else if (arg == "--warmup") {
      cfg.warmup = parse_size(next_value("--warmup"));
    } else if (arg == "--min-size") {
      cfg.min_size = parse_size(next_value("--min-size"));
    } else if (arg == "--max-size") {
      cfg.max_size = parse_size(next_value("--max-size"));
    } else if (arg == "--step") {
      cfg.step = parse_size(next_value("--step"));
    } else if (arg == "--threads") {
      cfg.threads = parse_size(next_value("--threads"));
    } else if (arg == "--batch") {
      cfg.batch = parse_size(next_value("--batch"));
    } else if (arg == "--queue-depth") {
      cfg.queue_depth = parse_size(next_value("--queue-depth"));
    } else if (arg == "--objects") {
      cfg.objects = parse_size(next_value("--objects"));
    } else if (arg == "--realloc-steps") {
      cfg.realloc_steps = parse_size(next_value("--realloc-steps"));
    } else if (arg == "--seed") {
      cfg.seed = parse_size(next_value("--seed"));
    } else if (arg == "--no-touch") {
      cfg.touch_memory = false;
    } else if (arg == "--random-order") {
      cfg.random_order = true;
    } else {
      std::ostringstream oss;
      oss << "unknown argument: " << arg;
      throw std::runtime_error(oss.str());
    }
  }

  if (cfg.min_size == 0 || cfg.max_size == 0 || cfg.step == 0 || cfg.threads == 0) {
    throw std::runtime_error("size/step/thread arguments must be non-zero");
  }
  if (cfg.min_size > cfg.max_size) {
    throw std::runtime_error("min-size must be <= max-size");
  }
  return cfg;
}

void print_config(const Config& cfg, const std::string& benchmark_name) {
  std::cout << "benchmark=" << benchmark_name
            << " iterations=" << cfg.iterations
            << " warmup=" << cfg.warmup
            << " min_size=" << cfg.min_size
            << " max_size=" << cfg.max_size
            << " step=" << cfg.step
            << " threads=" << cfg.threads
            << " batch=" << cfg.batch
            << " queue_depth=" << cfg.queue_depth
            << " objects=" << cfg.objects
            << " realloc_steps=" << cfg.realloc_steps
            << " touch_memory=" << (cfg.touch_memory ? 1 : 0)
            << " random_order=" << (cfg.random_order ? 1 : 0)
            << std::endl;
}

void touch_memory(void* ptr, std::size_t size) {
  if (ptr == nullptr || size == 0) {
    return;
  }
  auto* bytes = static_cast<unsigned char*>(ptr);
  constexpr std::size_t stride = 64;
  for (std::size_t i = 0; i < size; i += stride) {
    bytes[i] = static_cast<unsigned char>((i / stride) & 0xFFu);
  }
  bytes[size - 1] = 0xABu;
}

void pin_thread_if_requested(std::size_t index) {
#if defined(__linux__)
  const char* enabled = std::getenv("MALLOC_NEW_TEST_PIN");
  if (enabled == nullptr || std::strcmp(enabled, "1") != 0) {
    return;
  }
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  auto cpu_count = std::max(1u, std::thread::hardware_concurrency());
  CPU_SET(index % cpu_count, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#else
  (void)index;
#endif
}

void print_latency_summary(const std::string& name,
                           const std::vector<double>& samples_ns,
                           std::size_t operations) {
  if (samples_ns.empty()) {
    std::cout << name << " no_samples=1" << std::endl;
    return;
  }

  std::vector<double> sorted = samples_ns;
  std::sort(sorted.begin(), sorted.end());
  const auto pct = [&](double p) {
    std::size_t idx = static_cast<std::size_t>(p * static_cast<double>(sorted.size() - 1));
    return sorted[idx];
  };
  const double avg = std::accumulate(sorted.begin(), sorted.end(), 0.0) /
                     static_cast<double>(sorted.size());

  std::cout << std::fixed << std::setprecision(2)
            << name
            << " samples=" << sorted.size()
            << " operations=" << operations
            << " avg_ns=" << avg
            << " p50_ns=" << pct(0.50)
            << " p90_ns=" << pct(0.90)
            << " p99_ns=" << pct(0.99)
            << " min_ns=" << sorted.front()
            << " max_ns=" << sorted.back()
            << std::endl;
}

std::uint64_t fast_random(std::uint64_t& state) {
  state ^= state << 13;
  state ^= state >> 7;
  state ^= state << 17;
  return state;
}

void print_allocator_stats() {
  const auto stats = hp_get_allocator_stats();
  std::cout << "allocator_stats hugepage_refill_attempts=" << stats.hugepage_refill_attempts
            << " hugepage_refill_success=" << stats.hugepage_refill_success
            << " fallback_refill_success=" << stats.fallback_refill_success
            << std::endl;
}

std::vector<std::size_t> build_size_pattern(const Config& cfg) {
  std::vector<std::size_t> result;
  for (std::size_t size = cfg.min_size; size <= cfg.max_size; size += cfg.step) {
    result.push_back(size);
    if (cfg.max_size - size < cfg.step) {
      break;
    }
  }
  if (result.empty() || result.back() != cfg.max_size) {
    result.push_back(cfg.max_size);
  }
  return result;
}

}  // namespace bench
