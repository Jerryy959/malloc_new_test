# malloc_new_test

这是一个**直接面向 `malloc_new` 源码**的 VTune 热点分析测试仓库。当前仓库本身**不再提交 `malloc_new` 的实现文件**，而是在 CMake 配置阶段自动获取上游 `malloc_new`，然后让所有 benchmark 显式调用：

- `hp_malloc / hp_free / hp_realloc / hp_calloc`
- `malloc_new::compat::*`
- `malloc_new::hp_new / malloc_new::hp_delete`

这样你跑出的 VTune 热点会直接落在 `malloc_new` 的实现函数上，例如小对象线程缓存、hugetlb refill、fallback `mmap` 路径、`realloc` 复制路径等。

## 1. 当前接入方式

顶层 `CMakeLists.txt` 现在有两种接入模式：

1. **默认模式**：在 CMake 配置阶段自动 clone `malloc_new`
2. **覆盖模式**：通过 `-DMALLOC_NEW_SOURCE_DIR=/path/to/malloc_new` 指向你本地已有源码

默认 clone 参数：

- `MALLOC_NEW_GIT_REPOSITORY=https://github.com/Jerryy959/malloc_new.git`
- `MALLOC_NEW_GIT_TAG=main`

如果上游仓库后来切分支、改 tag，直接改这两个 CMake cache 变量即可。

## 2. 构建

### 2.1 自动 clone 上游 malloc_new

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 2.2 使用本地已有 malloc_new 源码

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DMALLOC_NEW_SOURCE_DIR=/path/to/malloc_new
cmake --build build -j
```

## 3. 可执行文件

构建后会生成以下 benchmark：

- `latency_fixed`
- `latency_size_sweep`
- `thread_cache_churn`
- `remote_free`
- `realloc_growth`
- `large_alloc`
- `fragmentation_reuse`
- `burst_contention`
- `new_delete_mix`

## 4. 每个 benchmark 实际调用的 malloc_new 接口

- `latency_fixed` / `latency_size_sweep` / `thread_cache_churn` / `remote_free` / `large_alloc` / `fragmentation_reuse` / `burst_contention`：通过 `malloc_new::compat::malloc/free` 间接走到 `hp_malloc/hp_free`
- `realloc_growth`：通过 `malloc_new::compat::malloc/realloc/free` 直接覆盖 `hp_realloc`
- `new_delete_mix`：通过 `malloc_new::hp_new<T>` 与 `malloc_new::hp_delete()` 直接测 C++ 对象路径

## 5. 示例运行

```bash
./build/latency_fixed --iterations 200000 --min-size 64
./build/thread_cache_churn --iterations 50000 --threads 8 --batch 64 --min-size 64 --max-size 256
./build/remote_free --iterations 200000 --min-size 64 --queue-depth 4096
./build/realloc_growth --iterations 100000 --min-size 64 --max-size 4096 --realloc-steps 6
./build/new_delete_mix --iterations 200000 --min-size 64
```

每个程序末尾都会输出 `allocator_stats`，方便你确认 `malloc_new` 是否触发了：

- `hugepage_refill_attempts`
- `hugepage_refill_success`
- `fallback_refill_success`

## 6. 推荐 VTune 命令

```bash
vtune -collect hotspots -result-dir vtune-results/latency_fixed -- ./build/latency_fixed --iterations 200000 --min-size 64
vtune -collect hotspots -result-dir vtune-results/remote_free -- ./build/remote_free --iterations 200000 --min-size 64 --queue-depth 4096
vtune -collect hotspots -result-dir vtune-results/burst_contention -- ./build/burst_contention --iterations 50000 --threads 8 --batch 128 --min-size 64
vtune -collect hotspots -result-dir vtune-results/realloc_growth -- ./build/realloc_growth --iterations 100000 --min-size 64 --max-size 4096 --realloc-steps 6
```

也可以直接运行脚本：

```bash
BUILD_DIR=build ./scripts/run_vtune_examples.sh
```

## 7. 参数说明

所有 benchmark 共享一套命令行参数：

- `--iterations N`
- `--warmup N`
- `--min-size N`
- `--max-size N`
- `--step N`
- `--threads N`
- `--batch N`
- `--queue-depth N`
- `--objects N`
- `--realloc-steps N`
- `--seed N`
- `--no-touch`
- `--random-order`

额外说明：

- 默认会触碰已分配内存，以便把首次写入带来的真实代价纳入观察。
- 如果你想更聚焦 allocator 自身而减少页触碰干扰，可加 `--no-touch`。
- 在 Linux 下设置 `MALLOC_NEW_TEST_PIN=1` 可以尝试为线程绑核，减少线程迁移噪声。

## 8. 详细设计报告

完整测试设计、tcmalloc/jemalloc 常见测试思路映射、以及后续如何根据 VTune 热点反推高延迟原因，见：

- [`docs/allocator_benchmark_report.md`](docs/allocator_benchmark_report.md)
