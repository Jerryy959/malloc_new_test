# malloc_new VTune 热点分析测试设计报告

## 1. 目标

这个仓库现在已经把 `malloc_new` 的源码直接引入并链接到 benchmark 中，因此测试程序不会再落到系统 `malloc/free`，而是会直接调用 `hp_malloc / hp_free / hp_realloc / hp_new` 等上游接口。

这个仓库的目标不是直接判定 `malloc_new` 是否“更快”，而是构建一组**适合 VTune 做热点、调用栈、锁竞争、线程本地缓存命中/失配、跨线程释放、大小类切换、系统调用放大**分析的独立程序。这样你后续可以：

1. 先用系统默认分配器跑一遍，建立基线。
2. 再通过 `LD_PRELOAD`、链接替换或你自己的构建方式切换到 `malloc_new`。
3. 用 VTune 对同一组二进制做热点分析，对比热点函数、CPU 时间、锁/同步热点、线程迁移和页故障。
4. 根据热点回到 `malloc_new` 代码中定位优化方向。

## 2. tcmalloc / jemalloc 常见测试思路总结

公开资料里，tcmalloc / jemalloc 的性能论证反复围绕以下几类场景：

### 2.1 小对象单线程低延迟路径

这类场景主要检验：

- 分配路径是否命中线程本地缓存；
- 元数据访问是否足够少；
- 小对象尺寸映射是否高效；
- `malloc` / `free` 最短路径上是否存在额外分支或锁。

对应本仓库测试：`latency_fixed`、`latency_size_sweep`。

### 2.2 多线程线程缓存压力

TCMalloc 的公开说明强调 thread cache / per-thread 或 per-CPU cache 是降低锁竞争的重要手段；jemalloc 的 arena / tcache 设计也同样关注线程局部快速路径。因此常见测试会放大量线程同时做小对象申请/释放，看：

- 是否出现中心锁热点；
- refill / flush 是否过于频繁；
- 尺寸类边界附近是否抖动；
- 多线程下 P99 是否明显恶化。

对应本仓库测试：`thread_cache_churn`、`burst_contention`。

### 2.3 跨线程释放（remote free）

很多 allocator 在“谁分配谁释放”时性能很好，但一旦出现 A 线程分配、B 线程释放，常常会暴露：

- 远程回收队列开销；
- 中央结构锁争用；
- span/page owner 转移开销；
- 线程缓存回收不及时导致尾延迟上升。

对应本仓库测试：`remote_free`。

### 2.4 `realloc` 扩容路径

jemalloc / tcmalloc 的用户与研究者也经常关注对象是否能原地扩容、是否需要拷贝、跨大小类扩容时是否引入额外碎片或系统调用。

对应本仓库测试：`realloc_growth`。

### 2.5 大对象路径 / `mmap` 路径

大对象往往绕开小对象缓存路径，直接暴露：

- `mmap` / `munmap` 调用；
- page map / extent map 管理；
- page fault / TLB 行为；
- 归还内核策略导致的延迟峰值。

对应本仓库测试：`large_alloc`。

### 2.6 碎片化与复用

Allocator 论文与工程 benchmark 经常用“先制造空洞，再申请不同大小对象”的方式观察：

- free list / size class 回收策略；
- coalescing 成本；
- reuse 失败后是否扩大系统分配；
- 延迟抖动是否随碎片程度上升。

对应本仓库测试：`fragmentation_reuse`。

### 2.7 C++ `new/delete` 场景

如果 `malloc_new` 的目标不仅是 C 风格接口，还有 `operator new/delete`，那么必须覆盖对象构造/析构路径，避免只测 `malloc/free`。

对应本仓库测试：`new_delete_mix`。

## 3. 本仓库程序说明

### 3.1 `latency_fixed`

用途：测固定大小对象的最短路径延迟。

建议：

- 先测 16B / 32B / 64B / 128B；
- 关闭与开启 `--no-touch` 分别测试“纯 allocator 路径”与“包含首次写入”；
- VTune 中重点看 `malloc_new` 的 fast path、size-class lookup、TLS 访问函数。

### 3.2 `latency_size_sweep`

用途：扫描一段尺寸区间，找延迟突刺点。

适合观察：

- size class 分界是否导致抖动；
- 某些大小是否触发慢路径；
- 小对象与中对象的切换点。

### 3.3 `thread_cache_churn`

用途：高频小对象批量分配/释放，观察线程缓存 refill/flush 热点。

适合观察：

- 中心 freelist 是否成为热点；
- 线程数上升后锁竞争是否出现；
- batch 大小时对延迟的影响。

### 3.4 `remote_free`

用途：一个线程分配、另一个线程释放。

适合观察：

- remote free 队列；
- span/page ownership 回迁；
- 线程缓存与中央缓存之间的数据移动。

### 3.5 `realloc_growth`

用途：模拟 buffer 扩容。

适合观察：

- 原地扩容概率；
- 数据拷贝路径；
- 跨 class 扩容时的热点函数。

### 3.6 `large_alloc`

用途：大对象高延迟路径。

适合观察：

- `mmap` / `munmap`；
- 内核页分配；
- huge page / page commit 行为。

### 3.7 `fragmentation_reuse`

用途：构造碎片后再复用。

适合观察：

- freelist 搜索；
- coalescing；
- span reuse / split / merge。

### 3.8 `burst_contention`

用途：所有线程同步起跑，制造瞬时争用峰值。

适合观察：

- 锁热点；
- 原子操作热点；
- 中央池 refill 临界区。

### 3.9 `new_delete_mix`

用途：覆盖 C++ 对象分配/释放路径。

适合观察：

- `operator new/delete` 重载路径；
- 小对象对象头与构造/析构耦合带来的额外开销。

## 4. 如何接入 malloc_new

当前仓库已经采用“源码直接引入”的方式接入 `malloc_new`，也就是通过 `add_subdirectory(third_party/malloc_new)` 构建并链接上游源码。这样做的好处是 VTune 热点会直接对应到 `malloc_new` 源文件。

如果你后续想切到自己的最新上游代码，仍可参考下面几种思路：

### 方案 A：`LD_PRELOAD`

如果 `malloc_new` 能编译成导出 `malloc/free/realloc/calloc` 与 `new/delete` 的共享库：

```bash
LD_PRELOAD=/path/to/libmalloc_new.so ./build/latency_fixed --iterations 100000 --min-size 64
```

优点：

- 不需要改测试代码；
- 同一二进制可直接对比 glibc / jemalloc / tcmalloc / malloc_new。

### 方案 B：链接替换

如果 `malloc_new` 提供静态库或共享库，也可以在构建时优先链接其符号。

### 方案 C：封装适配层

如果 `malloc_new` 暂时只有自定义 API（例如 `malloc_new_alloc()` / `malloc_new_free()`），建议你后续在这个仓库里再加一层 `allocator_adapter.hpp`，把 benchmark 调用统一映射过去。

## 5. VTune 推荐采样顺序

建议按下面顺序逐步分析：

1. `latency_fixed`：先看最短 fast path。
2. `latency_size_sweep`：找尺寸边界突刺。
3. `thread_cache_churn`：看 thread cache 是否有效。
4. `remote_free`：看 remote free 慢路径。
5. `burst_contention`：看多线程锁竞争峰值。
6. `large_alloc`：看大对象系统调用路径。
7. `fragmentation_reuse`：看碎片化后的复用。
8. `realloc_growth`：看扩容迁移。
9. `new_delete_mix`：看 C++ 路径。

## 6. VTune 观察指标建议

除了热点函数本身，我建议至少记录：

- Top-down：Front-end Bound / Bad Speculation / Back-end Bound / Retiring；
- hottest functions 的 inclusive / self time；
- spin / lock 时间；
- `mmap`, `munmap`, `brk`, page fault 相关热点；
- 原子操作热点；
- 线程迁移与 CPU 亲和性影响；
- 不同线程下 P50 / P90 / P99 延迟变化。

## 7. 高延迟的常见原因清单

等你跑完 VTune 后，若热点集中在以下位置，通常意味着这些问题：

1. **线程缓存命中率低**：频繁进入 central cache / global heap。
2. **size class 设计不合理**：某些尺寸边界导致 split / refill / internal fragmentation 放大。
3. **remote free 代价高**：跨线程释放时出现锁争用、转移队列过长或批量回收过晚。
4. **元数据布局差**：fast path 上 metadata cache miss 太多。
5. **过度原子化**：全局计数器、freelist head、span 状态修改产生 cache line ping-pong。
6. **大对象直接走系统调用**：`mmap/munmap`、页错误、TLB 抖动造成尾延迟。
7. **碎片化恢复成本高**：free list 搜索、coalescing、split/merge 太重。
8. **`realloc` 拷贝过多**：不能原地扩容，导致 memcpy 成为热点。
9. **NUMA / CPU 迁移问题**：线程在不同 CPU 间迁移，导致本地缓存优势被削弱。
10. **基准本身掺入首次写入成本**：若不开 `--no-touch`，热点可能来自页触碰而不是 allocator 本身。

## 8. 与 tcmalloc / jemalloc 对照测试建议

你最终最好让同一套程序至少跑四轮：

1. glibc allocator 基线；
2. jemalloc；
3. tcmalloc；
4. malloc_new。

然后按维度横向比较：

- 单线程平均延迟；
- 单线程 P99；
- 8/16 线程下延迟膨胀倍数；
- remote free 相比本地 free 的放大倍数；
- 大对象路径是否明显劣化；
- 哪个尺寸点开始出现突刺；
- 热点函数是否从 fast path 转移到 central path / syscalls。

## 9. 参考资料

- TCMalloc 文档：<https://gperftools.github.io/gperftools/tcmalloc.html>
- gperftools 项目：<https://github.com/gperftools/gperftools>
- jemalloc 项目：<https://github.com/jemalloc/jemalloc>
- mimalloc-bench（可参考额外 benchmark 组织方式）：<https://github.com/daanx/mimalloc-bench>

这些资料的共同点不是给出唯一 benchmark，而是反复围绕“小对象快路径、多线程缓存、跨线程释放、大对象、碎片化、真实工作负载”这些核心问题来验证 allocator 设计。
