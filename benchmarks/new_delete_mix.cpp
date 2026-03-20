#include "benchmark_common.hpp"

#include <array>
#include <iostream>
#include <memory>
#include <vector>

namespace {

struct Payload {
  std::array<std::uint64_t, 8> words{};
  std::vector<std::uint8_t> extra;

  explicit Payload(std::size_t size) : extra(size, 0x5A) {}
};

}  // namespace

int main(int argc, char** argv) {
  auto cfg = bench::parse_args(argc, argv);
  bench::print_config(cfg, "new_delete_mix");

  std::vector<double> samples;
  samples.reserve(cfg.iterations);

  for (std::size_t i = 0; i < cfg.iterations; ++i) {
    samples.push_back(bench::measure_ns([&] {
      auto* payload = malloc_new::hp_new<Payload>(cfg.min_size);
      payload->words[0] = i;
      malloc_new::hp_delete(payload);
    }));
  }

  bench::print_latency_summary("new_delete_cycle", samples, cfg.iterations);
  bench::print_allocator_stats();
  return 0;
}
