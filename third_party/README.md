# third_party

该目录按设计保持为空仓状态，不直接提交 `malloc_new` 源码。

`malloc_new` 会在 CMake 配置阶段自动 clone：

- 默认从 `https://github.com/Jerryy959/malloc_new.git` 获取；
- 也可以通过 `-DMALLOC_NEW_SOURCE_DIR=/local/path/to/malloc_new` 使用本地源码目录覆盖。
