# PriorityThreadPool

一个带优先级的 C++ 线程池实现。

## 特性

- 支持任务优先级
- 支持 `submit` 返回 `std::future`
- 优雅关闭
- C++17

## 编译

```bash
mkdir build && cd build
cmake ..
make