# 运行环境配置

## 操作系统
- Ubuntu 22.04 (Linux 6.8)

## 编译环境
- GCC/G++: 13.4.0 (支持 C++20/C++23)
- CMake: 3.22.1
- Make: GNU Make

## C++ 标准
- C++20

## 依赖库
- 标准C++库（libstdc++）
- POSIX Socket API（系统内置）
- pthread（多线程）

## 构建方式
```bash
mkdir build && cd build
cmake ..
make
```

## 运行方式
1. 启动服务端：`./server`
2. 启动客户端：`./client`
3. 在客户端输入SQL语句，输入 `exit` 退出

## 项目目录结构
```
database/
├── CMakeLists.txt      # 构建配置
├── config.md           # 本文件
├── 架构设计.md          # 架构设计文档
├── src/                # 源代码
├── tests/              # 单元测试
└── data/               # 运行时数据存储（自动生成）
```
