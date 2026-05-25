# MiniDB - 微型关系型数据库管理系统

> **MiniDB** 是一个对标 MySQL 核心功能的微型关系型数据库管理系统，从零实现了 SQL 解析、查询执行、B+树索引、数据持久化和 TCP 网络通信等数据库核心技术。本项目为《C++现代程序设计》课程期末大作业。

---

## 目录

- [项目概述](#项目概述)
- [技术栈](#技术栈)
- [系统架构](#系统架构)
- [功能特性](#功能特性)
- [快速开始](#快速开始)
- [使用说明书](#使用说明书)
  - [1. DDL 操作](#1-ddl-操作)
  - [2. DML 操作](#2-dml-操作)
  - [3. 视图操作](#3-视图操作)
  - [4. 使用技巧](#4-使用技巧)
  - [5. 完整生命周期示例](#5-完整生命周期示例)
- [项目结构](#项目结构)
- [核心模块详解](#核心模块详解)
- [测试](#测试)
- [设计约束](#设计约束)

---

## 项目概述

MiniDB 是一个**微型关系型数据库管理系统（Miniature Relational Database Management System）**，旨在通过从零实现数据库核心功能，深入理解关系数据库的原理与实现。项目对标 **MySQL** 的功能子集，覆盖了从 SQL 解析到数据持久化的完整链路。

**核心能力**：创建/删除数据库和表、插入/查询/更新/删除数据、B+树索引加速查询、视图创建/删除/查询、TCP/IP 网络通信、多行输入、多语句批处理、命令历史。

---

## 技术栈

| 类别 | 技术/工具 |
|:---|:---|
| 编程语言 | C++20 (`-std=c++20`) |
| 编译器 | GCC 13.4.0 |
| 构建系统 | CMake 3.22+ |
| 开发环境 | Linux (Ubuntu 22.04) |
| 网络通信 | POSIX Socket API (TCP/IP) |
| 数据格式 | JSON (自定义序列化/反序列化) |
| 数据存储 | 二进制文件 (宿主文件系统) |
| 索引结构 | B+树 (阶数4，文件持久化) |
| 容器实现 | 自研 `ArrayList<T>` (禁止使用 STL 容器) |
| 线程模型 | POSIX Threads |
| CLI 交互 | GNU Readline (历史命令/行编辑) |
| 代码生成 | Vibe Coding (AI 辅助编码) |

---

## 系统架构

### 整体架构（C/S 两层架构）

```
┌─────────────────────────────────────────────────────┐
│                    MiniDB 系统                        │
├──────────────────┬──────────────────────────────────┤
│  客户端 (Client)  │      服务端 (Server)              │
├──────────────────┼──────────────────────────────────┤
│  CLI 交互界面     │  SQL 解析器 (SQLParser)            │
│  (stdin/stdout)  │    ↓ AST                           │
│       ↓          │  执行引擎 (Executor)                │
│  网络客户端       │    ↓                               │
│  (Socket)        │  存储引擎 → B+树 → 文件系统         │
└──────────────────┴──────────────────────────────────┘
```

### 通信协议

- **传输层**：TCP/IP，默认端口 23333
- **应用层**：JSON 文本格式，以 `\n` 换行符作为消息结束标志

**请求格式**：`{"sql":"create database person"}`

**响应格式**：
```json
{"code":0,"message":"Query OK","affectedRows":1,
 "columns":[{"name":"id","type":"int","primary":true}],
 "rows":[[1001,"peter"]]}
```

### 数据流

```
用户输入SQL → Client (CLI) → TCP/JSON → Server
    → SQLParser (词法分析→语法分析→AST)
    → Executor (语义分析→执行)
    → StorageEngine / BPlusTree / 文件系统
    → ResultSet (JSON) → TCP → Client → 格式化显示
```

### 存储结构

```
data/                           # 数据根目录
├── <dbname>/                   # 数据库目录
│   ├── <tablename>.dat        # 表数据文件（二进制定长记录）
│   ├── <tablename>.idx        # B+树索引文件
│   └── schema.json            # 元数据文件
```

---

## 功能特性

### DDL（数据定义语言）

| 语句 | 语法 | 说明 |
|:---|:---|:---|
| CREATE DATABASE | `create database <dbname>` | 创建数据库目录 |
| DROP DATABASE | `drop database <dbname>` | 删除数据库及其所有文件 |
| USE | `use <dbname>` | 切换当前数据库 |
| CREATE TABLE | `create table <name> (<col> <type> [primary], ..., foreign key (<col>) references <table>(<col>) [on delete cascade])` | 创建表，支持 int/string，主键自动建B+树索引，支持外键约束与级联删除 |
| DROP TABLE | `drop table <name>` | 删除表及索引文件 |
| CREATE VIEW | `create view <name> as select <cols> from <table> [where <cond>]` | 创建视图，基于源表的虚拟表 |
| DROP VIEW | `drop view <name>` | 删除视图定义 |

### DML（数据操作语言）

| 语句 | 语法 | 说明 |
|:---|:---|:---|
| INSERT | `insert <table> values(<val>, ...)` | 插入数据，字符串用双引号 |
| SELECT | `select <col>\|* from <table> [where <cond>]` | 条件查询，支持 = < > |
| UPDATE | `update <table> set <col>=<val> [where <cond>]` | 条件更新 |
| DELETE | `delete <table> [where <cond>]` | 条件删除 |

### B+树索引优化

- 主键自动建立 B+树索引（阶数 4）
- 主键等值查询 `where id = xxx` 使用索引加速 O(log n)

---

## 快速开始

### 环境要求

- GCC/G++ 11.0+ (支持 C++20)
- CMake 3.10+
- Linux (POSIX Socket)
- libreadline-dev (客户端 CLI 历史命令支持)

### 构建

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### 运行

**启动服务端**（终端1）：
```bash
cd build && ./server
# Server started on port 23333
# Waiting for connections...
```

**启动客户端**（终端2）：
```bash
cd build && ./client

# MiniDB Client
# Type SQL statements or 'exit' to quit
# minidb>
```

**运行测试**：
```bash
cd build && ./test_minidb
```

---

## 使用说明书

### 1. DDL 操作

```sql
minidb> create database school
Database created

minidb> use school
Database changed

minidb> create table student (id int primary, name string, age int)
Success

minidb> drop table student
Success

minidb> drop database school
Success
```

### 2. DML 操作

```sql
-- 插入数据
minidb> insert student values(1001, "peter", 18)
1 row inserted

-- 无条件查询
minidb> select * from student
+------+-------+-----+
| id   | name  | age |
+------+-------+-----+
| 1001 | peter | 18  |
| 1002 | john  | 20  |
+------+-------+-----+
2 rows in set

-- 条件查询（使用索引）
minidb> select name from student where id = 1001
+-------+
| name  |
+-------+
| peter |
+-------+
1 row in set

-- 条件查询（范围）
minidb> select * from student where age > 18
+------+------+-----+
| id   | name | age |
+------+------+-----+
| 1002 | john | 20  |
+------+------+-----+
1 row in set

-- 更新数据
minidb> update student set name = "peter_updated" where id = 1001
1 row updated

-- 删除数据
minidb> delete student where id = 1002
1 row deleted

-- 退出
minidb> exit
Bye
```

### 3. 视图操作

```sql
-- 创建视图
minidb> create view v_student as select id, name from student
View created

-- 从视图查询（展开为底层表查询）
minidb> select * from v_student
+---------+--------+
| id      | name   |
+---------+--------+
| 2024001 | 张三 |
| 2024002 | 李四 |
+---------+--------+
2 rows in set

-- 从视图查询带条件
minidb> select * from v_student where id = 2024001
+---------+--------+
| id      | name   |
+---------+--------+
| 2024001 | 张三 |
+---------+--------+
1 row in set

-- 创建视图（带 WHERE 条件）
minidb> create view v_adult as select * from student where age > 18
View created

-- 查询带条件视图
minidb> select * from v_adult
+---------+--------+-----+
| id      | name   | age |
+---------+--------+-----+
| 2024002 | 李四 | 22  |
+---------+--------+-----+
1 row in set

-- 删除视图
minidb> drop view v_student
View dropped
```

### 4. 使用技巧

本节介绍 MiniDB 客户端 CLI 提供的增强交互功能。

#### 4.1 多行输入

语句可以跨多行输入，以 `;` 作为语句结束标志。当一行未包含 `;` 时，提示符变为 `->` 表示续入模式：

```sql
minidb> create table student (
->   id int primary,
->   name string,
->   age int
-> );
Success
```

#### 4.2 多条语句批处理

在一行或多行中输入多条语句，用 `;` 分隔，系统会逐条执行并分别显示结果：

```sql
minidb> create database test; use test; create table t1 (id int primary);
Database created
Database changed
Success
```

#### 4.3 命令历史

使用 ↑/↓ 箭头键浏览历史输入过的 SQL 语句（由 GNU Readline 库提供支持）：

- **↑**：调出上一条历史命令
- **↓**：回到下一条历史命令
- **←/→**：行内左右移动编辑
- **Ctrl+A**：跳转到行首
- **Ctrl+E**：跳转到行尾
- **Ctrl+U**：清除整行

所有执行过的 SQL 语句（含 `exit` 命令前的最后一次）自动保存到历史记录中。

---

### 5. 完整生命周期示例

```bash
# 终端1：启动服务端
$ cd build && ./server
Server started on port 23333
Waiting for connections...

# 终端2：启动客户端
$ cd build && ./client
Connected to MiniDB server at 127.0.0.1:23333

minidb> create database school
Database created

minidb> use school
Database changed

minidb> create table student (id int primary, name string, age int)
Success

minidb> insert student values(2024001, "张三", 20)
1 row inserted
minidb> insert student values(2024002, "李四", 21)
1 row inserted
minidb> insert student values(2024003, "王五", 19)
1 row inserted

minidb> select * from student
+---------+--------+-----+
| id      | name   | age |
+---------+--------+-----+
| 2024001 | 张三 | 20  |
| 2024002 | 李四 | 21  |
| 2024003 | 王五 | 19  |
+---------+--------+-----+
3 rows in set

minidb> select name from student where id = 2024002
+--------+
| name   |
+--------+
| 李四 |
+--------+
1 row in set

minidb> update student set age = 22 where name = "李四"
1 row updated

minidb> delete student where age < 20
1 row deleted

minidb> select * from student
+---------+--------+-----+
| id      | name   | age |
+---------+--------+-----+
| 2024001 | 张三 | 20  |
| 2024002 | 李四 | 22  |
+---------+--------+-----+
2 rows in set

minidb> exit
Bye
```

---

## 项目结构

```
database/                           # 项目根目录
├── CMakeLists.txt                  # CMake 构建配置
├── README.md                       # 本文件
├── config.md                       # 运行环境配置
├── 任务书.md                        # 课程任务书
├── 架构设计.md                      # 架构设计文档
├── 项目报告.md                      # 项目报告
│
├── src/                            # 源代码
│   ├── server_main.cpp             # 服务端入口
│   ├── client_main.cpp             # 客户端入口
│   ├── common/                     # 公共模块
│   │   ├── Common.h                # 类型/枚举/工具函数
│   │   ├── ArrayList.h             # 自定义动态数组容器
│   │   ├── Value.h                 # 数据值类型(int/string)
│   │   ├── Column.h                # 列定义
│   │   ├── Row.h                   # 行数据容器
│   │   └── ResultSet.h             # 结果集+JSON序列化
│   ├── index/                      # 索引模块
│   │   └── BPlusTree.h             # B+树实现
│   ├── storage/                    # 存储引擎
│   │   ├── StorageEngine.h         # 数据库/文件管理
│   │   └── Table.h                 # 表CRUD操作
│   ├── parser/                     # SQL解析器
│   │   ├── ASTNode.h               # 抽象语法树
│   │   └── SQLParser.h             # 词法/语法分析
│   ├── executor/                   # 执行引擎
│   │   └── Executor.h              # AST→数据库操作
│   └── network/                    # 网络通信
│       ├── Socket.h                # TCP封装
│       ├── Server.h                # 服务端
│       └── Client.h                # 客户端
│
├── tests/                          # 单元测试
│   └── test_main.cpp               # 42个测试用例
│
├── build/                          # 构建输出
│   ├── server                      # 服务端可执行文件
│   ├── client                      # 客户端可执行文件
│   └── test_minidb                 # 单元测试可执行文件
│
└── data/                           # 运行时数据（自动生成）
```

---

## 核心模块详解

### 1. 自定义容器 ArrayList

**为什么需要？** 任务要求禁止使用 STL 容器（vector/list/map/set等）。

**关键设计**：
- 原始指针 + 动态内存分配
- 初始容量 4，2 倍扩容策略
- 支持迭代器、const 版本 operator[]、find/erase_at/insert_at 等操作

### 2. B+树索引

**数据结构**：阶数 4 的 B+树
- 内部节点：键 + 子节点指针（文件偏移）
- 叶子节点：键 + 数据偏移 + nextLeaf链表
- 节点持久化到 .idx 文件

**操作**：insert（自动分裂）、search（O(log n)）、searchRange、remove、getAll

### 3. SQL 解析器

**方式**：手写递归下降解析器
- 词法分析：提取关键字和标识符
- 语法分析：根据产生式构建 AST
- 支持 11 种语句类型 + where 条件（含 CREATE/DROP VIEW）

### 4. 存储引擎

- 数据文件：二进制定长记录
- 索引文件：B+树序列化
- 元数据：CSV 格式 schema.json
- B+树索引加速主键等值查询

### 5. 视图模块

**原理**：视图是一种虚拟表，其内容由查询定义（存储的 SELECT 语句），不占用物理存储空间。

**实现方式**：
- 视图定义存储在 `schema.json` 中，以 `VIEW:` 前缀标识
- 格式：`VIEW:viewname,col1,col2,...,sourcetable[,condcol,condop,condval]`
- 查询视图时，`Executor` 检测表名是否为视图，自动展开为底层表查询
- 支持带 WHERE 条件的视图定义，用户查询条件可与视图条件组合

### 6. 网络通信

- TCP Socket API
- JSON 格式消息
- 服务端循环 accept → recv/send
- 客户端 CLI 交互

---

## 测试

### 测试结果

```
=======================================
  MiniDB Unit Tests
=======================================
Test 1: ArrayList basic operations ... PASS
...
Test 60: Drop FK database ... PASS
=======================================
  Results: 61/61 tests passed
=======================================
```

### 覆盖范围

| 模块 | 用例数 | 覆盖内容 |
|:---|:---:|:---|
| ArrayList | 4 | 基本操作、查找、拷贝、迭代器 |
| Value | 4 | 创建、比较、序列化 |
| SQLParser | 16 | 全部11种语句+where+视图解析+外键解析 |
| BPlusTree | 2 | 插入查找、中序遍历 |
| 集成测试 | 18 | 完整数据库生命周期+视图CRUD |
| 外键约束 | 18 | 外键创建/插入校验/引用拒绝/级联删除/DROP保护 |
| **总计** | **60** | **全部通过** |

---

## 设计约束

1. **禁止 STL 容器**：自研 ArrayList<T>
2. **C++20 标准**：-std=c++20
3. **Linux 环境**：基于 POSIX API
4. **CMake 构建**
5. **允许使用的库**：
   - ✅ std::string / iostream / fstream
   - ✅ POSIX Socket / Threads
   - ❌ std::vector / std::list / std::map / std::set
