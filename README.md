# 数据库异步连接池（DB Async Pool）

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Ubuntu%2024.04-lightgrey.svg)
![MySQL](https://img.shields.io/badge/MySQL-8.0-orange.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Status](https://img.shields.io/badge/status-stable-brightgreen.svg)

> 一个基于 C++17 实现的 MySQL 异步连接池，通过“连接池 + 线程池 + 任务队列”模型，让应用线程提交 SQL 后立即返回，由内部工作线程完成阻塞的数据库操作，并通过回调或 `std::future` 返回结果。

---

## 项目简介

在传统的同步数据库访问中，应用线程调用 `mysql_query()` 后会一直阻塞，直到数据库返回结果。在高并发场景下，这会导致大量线程被 I/O 阻塞，吞吐量急剧下降。

本项目实现了一个**数据库异步连接池**，它解决了以下问题：

- **连接复用**：预先创建多条 MySQL 物理连接，避免频繁 TCP 握手和认证开销。
- **异步执行**：应用线程只需将 SQL 任务投递到队列即可立即返回，不阻塞业务逻辑。
- **并发控制**：通过固定数量的工作线程和连接，防止数据库过载。
- **结果获取灵活**：支持回调（callback）和 `std::future` 两种风格，适应不同编程习惯。

**亮点**：

- 纯 C++17 实现，无第三方框架依赖（仅依赖 `libmysqlclient`）。
- 单例连接池，全局唯一，线程安全。
- 支持优雅关闭（`shutdown()`），工作线程可及时退出。
- 模块化设计：`BlockingQueue`、`MySQLConn`、`SQLTask`、`MySQLConnPool` 各司其职，易于扩展。

---

## 功能特性

- ✅ **连接池管理**：初始化时创建 N 条 MySQL 连接，空闲时放入队列，使用时借出，用完归还。
- ✅ **线程池复用**：固定 M 个工作线程，循环从任务队列取任务，避免频繁创建销毁线程。
- ✅ **线程安全阻塞队列**：基于 `std::mutex` + `std::condition_variable` 实现，支持超时等待。
- ✅ **异步查询**：`asyncQuery()` 回调风格，提交后立即返回。
- ✅ **异步 Future**：`asyncQueryFuture()` 返回 `std::future<QueryResult>`，可同步等待结果。
- ✅ **异步更新**：`asyncUpdate()` 支持 INSERT/UPDATE/DELETE，返回受影响行数。
- ✅ **单例模式**：`MySQLConnPool::instance()` 全局唯一，线程安全（C++11 magic static）。
- ✅ **优雅关闭**：`shutdown()` 等待工作线程退出，释放所有连接。
- ✅ **连接探活与重连**：`isAlive()` 和 `refresh()` 保证连接可用性。
- ✅ **字符集设置**：默认使用 `utf8mb4`，支持中文。

---

## 环境依赖

| 项目 | 要求 |
|------|------|
| 操作系统 | Linux（推荐 Ubuntu 24.04） |
| 编译器 | g++ 9+ 或 clang++ 10+，支持 C++17 |
| 构建工具 | CMake 3.10+ |
| MySQL | MySQL Server 8.0+ |
| 开发库 | `libmysqlclient-dev` |
| 其他 | `pthread`（Linux 自带） |
| 编辑器 | VSCode（可选） |

**安装依赖（Ubuntu 24.04）**：

```bash
sudo apt update
sudo apt install -y mysql-server libmysqlclient-dev cmake g++ build-essential
```

---

## 安装 / 编译步骤

### 1. 启动 MySQL 并创建测试数据库

```bash
sudo service mysql start
sudo mysql_secure_installation   # 按提示设置 root 密码，例如 123456
```

登录 MySQL：

```bash
mysql -u root -p
```

执行以下 SQL：

```sql
CREATE DATABASE IF NOT EXISTS test_db;
USE test_db;
CREATE TABLE IF NOT EXISTS users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    age INT NOT NULL
);
INSERT INTO users (name, age) VALUES ('Alice', 25), ('Bob', 30), ('Charlie', 22);
SELECT * FROM users;
EXIT;
```

### 2. 克隆或创建项目目录

```bash
cd ~/projects
mkdir -p db_async_pool/{include,src,build}
cd db_async_pool
```

### 3. 创建 `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.10)
project(DbAsyncPool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MYSQL_INCLUDE_DIRS /usr/include/mysql)
set(MYSQL_LIBRARIES mysqlclient)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${MYSQL_INCLUDE_DIRS}
)

file(GLOB SOURCES "src/*.cpp")

add_executable(db_async_pool ${SOURCES})

target_link_libraries(db_async_pool
    PRIVATE
    ${MYSQL_LIBRARIES}
    pthread
)
```

### 4. 创建源文件

按照 [项目结构](#项目结构) 中的文件列表，创建 `include/` 和 `src/` 下的所有文件。完整代码见 [快速开始](#快速开始--使用示例) 或项目仓库。

### 5. 编译

```bash
cd ~/projects/db_async_pool/build
cmake ..
make
```

编译成功后，`build/` 目录下会生成可执行文件 `db_async_pool`。

---

## 快速开始 / 使用示例

以下是一个最小可运行示例（`src/main.cpp`）：

```cpp
#include "MySQLConnPool.h"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    // 1. 获取单例并初始化连接池
    auto& pool = MySQLConnPool::instance();
    if (!pool.init("127.0.0.1", "root", "123456", "test_db", 4, 4, 3306)) {
        std::cerr << "Pool init failed!" << std::endl;
        return 1;
    }

    // 2. 回调风格异步查询
    pool.asyncQuery("SELECT * FROM users", [](QueryResult result) {
        std::cout << "Callback result:" << std::endl;
        for (auto& row : result) {
            for (auto& field : row) std::cout << field << "\t";
            std::cout << std::endl;
        }
    });

    // 3. Future 风格异步查询（可等待）
    auto future = pool.asyncQueryFuture("SELECT COUNT(*) FROM users");
    QueryResult count = future.get();
    std::cout << "User count: " << count[0][0] << std::endl;

    // 4. 异步更新
    pool.asyncUpdate("INSERT INTO users (name, age) VALUES ('Dave', 28)");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 5. 再次查询验证
    auto f2 = pool.asyncQueryFuture("SELECT * FROM users");
    QueryResult all = f2.get();
    std::cout << "Total users: " << all.size() << std::endl;

    // 6. 关闭连接池
    pool.shutdown();
    return 0;
}
```

**编译并运行**：

```bash
cd ~/projects/db_async_pool/build
cmake .. && make
./db_async_pool
```

**预期输出**：

```
[Pool] Created 4 connections.
[Pool] Started 4 worker threads.
Callback result:
1	Alice	25	
2	Bob	30	
3	Charlie	22	
User count: 3
Total users: 4
[Pool] Shutdown complete.
```

---

## 项目结构

```
db_async_pool/
├── CMakeLists.txt              # CMake 构建脚本
├── include/
│   ├── BlockingQueue.h         # 线程安全阻塞队列（模板类）
│   ├── MySQLConn.h             # 单条 MySQL 连接封装
│   ├── SQLTask.h               # SQL 任务定义（回调 / promise）
│   └── MySQLConnPool.h         # 异步连接池核心类（单例）
├── src/
│   ├── MySQLConn.cpp           # MySQLConn 实现
│   ├── MySQLConnPool.cpp       # MySQLConnPool 实现
│   └── main.cpp                # 测试入口
└── build/                      # 编译输出目录（CMake 生成）
    ├── Makefile
    └── db_async_pool           # 可执行文件
```

**关键文件说明**：

- `BlockingQueue.h`：提供 `push`、`pop`、`pop_with_timeout` 等接口，是任务队列和连接队列的基础。
- `MySQLConn.h/.cpp`：封装 `mysql_init`、`mysql_real_connect`、`mysql_query`、`mysql_store_result` 等 C API。
- `SQLTask.h`：定义 `TaskType`（QUERY/UPDATE）、`QueryCallback`，以及 `execute(MySQLConn*)` 方法。
- `MySQLConnPool.h/.cpp`：单例类，管理连接池、线程池、任务队列，对外提供异步接口。

---

## 核心设计 / 原理说明

### 整体架构

```
┌─────────────────────────────────────────────────────┐
│                    应用线程                          │
│   pool->asyncQuery("SELECT ...", callback);         │
│   （立即返回，不阻塞）                                │
└──────────────────────┬──────────────────────────────┘
                       │ 投递任务
                       ▼
┌─────────────────────────────────────────────────────┐
│              任务队列 (BlockingQueue)                │
│   std::shared_ptr<SQLTask>                          │
└──────────────────────┬──────────────────────────────┘
                       │ 工作线程取任务
                       ▼
┌─────────────────────────────────────────────────────┐
│           工作线程池 (N 个线程)                       │
│   每个线程：取出任务 → 获取连接 → 执行 SQL → 回调     │
└──────────────────────┬──────────────────────────────┘
                       │ 获取/归还
                       ▼
┌─────────────────────────────────────────────────────┐
│              连接池 (M 个 MySQL 连接)                 │
│   BlockingQueue<MySQLConn*> idle_queue_             │
└─────────────────────────────────────────────────────┘
```

### 关键设计点

1. **为什么“异步”需要线程池？**  
   `libmysqlclient` 默认使用阻塞 I/O，`mysql_query()` 会阻塞当前线程。异步连接池将阻塞操作转移到工作线程，应用线程只负责投递任务。

2. **连接池与线程池的配合**  
   - 连接池管理 M 条物理连接，避免频繁建立连接。
   - 线程池管理 N 个工作线程，每个线程从任务队列取任务，借用连接执行 SQL，然后归还。
   - 任务队列是应用线程与工作线程之间的桥梁。

3. **`pop_with_timeout` 的作用**  
   工作线程使用 `pop_with_timeout(task, 500)` 带超时等待任务。当 `shutdown()` 将 `running_` 设为 `false` 时，线程最多等待 500ms 即可退出，不会永久阻塞。

4. **单例模式**  
   使用 C++11 magic static（`static MySQLConnPool pool;`）保证线程安全的懒加载单例。

5. **结果传递**  
   - 回调风格：`SQLTask` 持有 `QueryCallback`，执行后直接调用。
   - Future 风格：`SQLTask` 持有 `std::shared_ptr<std::promise<QueryResult>>`，执行后 `set_value`。

---

## API 说明

### `MySQLConnPool`（核心类，单例）

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `static MySQLConnPool& instance()` | 无 | `MySQLConnPool&` | 获取全局唯一实例 |
| `bool init(host, user, password, dbname, pool_size=4, thread_count=4, port=3306)` | 连接参数、池大小、线程数 | `bool` | 初始化连接池，创建连接和线程。成功返回 `true` |
| `void asyncQuery(const std::string& sql, QueryCallback callback)` | SQL、回调函数 | `void` | 异步查询，结果通过回调返回 |
| `std::future<QueryResult> asyncQueryFuture(const std::string& sql)` | SQL | `std::future<QueryResult>` | 异步查询，返回 future，可 `.get()` 等待 |
| `void asyncUpdate(const std::string& sql, QueryCallback callback = nullptr)` | SQL、可选回调 | `void` | 异步更新，受影响行数通过回调（空结果）或忽略 |
| `void shutdown()` | 无 | `void` | 优雅关闭，等待工作线程退出，释放连接 |
| `size_t idleCount() const` | 无 | `size_t` | 当前空闲连接数 |

**异常**：`init()` 失败返回 `false`，不抛异常；任务执行中的异常由工作线程捕获并打印。

### `MySQLConn`

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `MySQLConn()` | 无 | - | 构造，调用 `mysql_init` |
| `~MySQLConn()` | 无 | - | 析构，关闭连接 |
| `bool connect(host, user, password, dbname, port=3306)` | 连接参数 | `bool` | 建立物理连接 |
| `QueryResult query(const std::string& sql)` | SQL | `QueryResult` | 执行查询，返回二维字符串向量 |
| `int update(const std::string& sql)` | SQL | `int` | 执行更新，返回受影响行数（-1 表示失败） |
| `bool isAlive()` | 无 | `bool` | 使用 `mysql_ping` 探活 |
| `bool refresh()` | 无 | `bool` | 关闭并重新连接 |
| `MYSQL* raw()` | 无 | `MYSQL*` | 获取原始指针 |
| `std::chrono::steady_clock::time_point lastActiveTime() const` | 无 | 时间点 | 最后活跃时间 |

### `SQLTask`

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `SQLTask(TaskType type, std::string sql, QueryCallback cb)` | 类型、SQL、回调 | - | 构造回调风格任务 |
| `SQLTask(TaskType type, std::string sql, std::shared_ptr<std::promise<QueryResult>> promise)` | 类型、SQL、promise | - | 构造 future 风格任务 |
| `void execute(MySQLConn* conn)` | 连接指针 | `void` | 由工作线程调用，执行 SQL 并传递结果 |
| `TaskType type() const` | 无 | `TaskType` | 获取任务类型 |
| `const std::string& sql() const` | 无 | `const std::string&` | 获取 SQL 语句 |

### `BlockingQueue<T>`（模板类）

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `void push(T item)` | 元素 | `void` | 入队并唤醒一个消费者 |
| `T pop()` | 无 | `T` | 阻塞出队，队列空时等待 |
| `bool pop_with_timeout(T& item, int timeout_ms)` | 引用、超时毫秒 | `bool` | 带超时出队，超时返回 `false` |
| `bool empty() const` | 无 | `bool` | 是否为空 |
| `size_t size() const` | 无 | `size_t` | 元素个数 |

**类型别名**：`QueryResult = std::vector<std::vector<std::string>>`，`QueryCallback = std::function<void(QueryResult)>`。

---

## 运行测试

项目自带一个综合测试 `src/main.cpp`，覆盖以下场景：

1. 回调风格异步查询
2. Future 风格异步查询
3. 异步更新（INSERT）
4. 并发查询（8 个查询同时发起）
5. 优雅关闭

**运行方式**：

```bash
cd ~/projects/db_async_pool/build
./db_async_pool
```

**预期结果**：

- 输出 `[Pool] Created 4 connections.` 和 `[Pool] Started 4 worker threads.`
- 回调查询输出 users 表内容
- Future 查询输出用户数量
- 更新后用户总数增加
- 并发查询全部完成
- 输出 `[Pool] Shutdown complete.` 并正常退出

如果输出与预期不符，请参考 [常见问题 FAQ](#常见问题-faq)。

---

## 常见问题 FAQ

### 1. `Can't connect to MySQL server on '127.0.0.1'`

- 检查 MySQL 服务是否启动：`sudo service mysql status`，未启动则 `sudo service mysql start`。
- 检查用户名/密码是否正确，可手动 `mysql -u root -p` 登录验证。
- 检查 `test_db` 数据库是否存在。

### 2. 编译时报 `mysql.h: No such file or directory`

- 安装 `libmysqlclient-dev`：`sudo apt install libmysqlclient-dev`。
- 确认 `/usr/include/mysql/mysql.h` 存在。若路径不同，修改 `CMakeLists.txt` 中的 `MYSQL_INCLUDE_DIRS`。

### 3. 链接时报 `undefined reference to mysql_init`

- 确认 `CMakeLists.txt` 中 `target_link_libraries` 包含 `${MYSQL_LIBRARIES}`（即 `mysqlclient`）。
- 清理后重新编译：`rm -rf build/* && cmake .. && make`。

### 4. 程序卡住不退出

- 可能是 `shutdown()` 时工作线程阻塞在 `acquireConnection()` 的 `pop()` 上。可将 `acquireConnection()` 改为带超时循环，并在 `running_` 为 `false` 时返回 `nullptr`：

```cpp
MySQLConn* MySQLConnPool::acquireConnection() {
    MySQLConn* conn = nullptr;
    while (running_) {
        if (idle_queue_.pop_with_timeout(conn, 500)) {
            return conn;
        }
    }
    return nullptr;
}
```

### 5. 中文乱码

- 代码已设置 `utf8mb4`。确保数据库和表字符集也是 `utf8mb4`：

```sql
ALTER DATABASE test_db CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
ALTER TABLE users CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
```

### 6. `find_package(MySQL)` 报错

- 本项目 `CMakeLists.txt` 已改为手动指定路径，不依赖 `find_package`。若仍报错，检查 `MYSQL_INCLUDE_DIRS` 和 `MYSQL_LIBRARIES` 是否正确。

---

## 贡献指南

欢迎任何形式的贡献！

### 提 Issue

- 描述清楚问题：环境（OS、编译器版本、MySQL 版本）、复现步骤、预期结果、实际结果。
- 附上完整错误日志。

### 提 Pull Request

1. Fork 本仓库。
2. 新建分支：`git checkout -b feature/your-feature`。
3. 遵循现有代码风格：
   - C++17 标准，使用 `std::` 前缀。
   - 头文件使用 `#ifndef` 卫哨。
   - 类名大驼峰，函数名小驼峰，成员变量末尾加 `_`。
   - 提交前确保 `cmake .. && make` 编译通过。
4. 提交 PR，描述改动内容和目的。

### 代码风格要求

- 缩进 4 空格。
- 每行不超过 100 字符。
- 关键逻辑添加注释。
- 不引入不必要的第三方依赖。

---

## 许可证 License

本项目采用 **MIT License** 开源协议。

```
MIT License

Copyright (c) 2025 [你的名字]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 作者与联系方式

- **作者**：[MisakaMikoto-QAQ]
- **邮箱**：[3128204593@qq.com]
- **GitHub**：[https://github.com/MisakaMikoto-QAQ](https://github.com/MisakaMikoto-QAQ)
- **项目地址**：[https://github.com/MisakaMikoto-QAQ/db_async_pool](https://github.com/MisakaMikoto-QAQ/db_async_pool)

---

## 致谢 / 参考

- [MySQL 8.0 C API Reference](https://dev.mysql.com/doc/c-api/8.0/en/)
- [C++ Reference: std::condition_variable](https://en.cppreference.com/w/cpp/thread/condition_variable)
- [C++ Reference: std::future](https://en.cppreference.com/w/cpp/thread/future)
- [CMake Documentation](https://cmake.org/documentation/)
- 项目一：手写线程池
- 项目二：手写内存池

感谢所有为本项目提供灵感和帮助的开源项目与文章。

---

## 更新日志 / 路线图

### v1.0.0 (2025-10-15)

- 初始版本发布。
- 实现基于线程池 + 任务队列的 MySQL 异步连接池。
- 支持回调与 `std::future` 两种结果获取方式。
- 支持异步查询、异步更新、优雅关闭。
- 提供完整测试示例。

### 路线图

- [ ] 连接有效性检测与自动重连（在归还连接时探活）。
- [ ] 空闲连接回收（后台线程定期清理超时空闲连接）。
- [ ] 动态扩容（任务积压时临时增加连接）。
- [ ] 支持 MySQL 8.0 异步 C API（`mysql_real_query_nonblocking` 等）。
- [ ] 添加单元测试（Google Test）。
- [ ] 提供 Docker 构建环境。

---

**如果这个项目对你有帮助，欢迎 Star ⭐ 和 Fork！**