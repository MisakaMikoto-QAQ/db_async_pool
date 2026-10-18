#include "MySQLConnPool.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

int main() {
    // 1. 初始化连接池
    auto& pool = MySQLConnPool::instance();
    // 注意：将 "你的密码" 替换为你在第二章设置的 MySQL root 密码
    if (!pool.init("127.0.0.1", "root", "wbt1114HT!", "test_db", 4, 4, 3306)) {
        std::cerr << "Pool init failed!" << std::endl;
        return 1;
    }

    // 2. 回调风格的异步查询
    std::cout << "\n=== Callback style ===" << std::endl;
    pool.asyncQuery("SELECT * FROM users", [](QueryResult result) {
        std::cout << "Query result:" << std::endl;
        for (auto& row : result) {
            for (auto& field : row) {
                std::cout << field << "\t";
            }
            std::cout << std::endl;
        }
    });

    // 3. Future 风格的异步查询（可同步等待结果）
    std::cout << "\n=== Future style ===" << std::endl;
    auto future = pool.asyncQueryFuture("SELECT COUNT(*) FROM users");
    QueryResult count_result = future.get();
    if (!count_result.empty()) {
        std::cout << "User count: " << count_result[0][0] << std::endl;
    }

    // 4. 异步更新
    std::cout << "\n=== Async update ===" << std::endl;
    pool.asyncUpdate("INSERT INTO users (name, age) VALUES ('Dave', 28)");
    // 稍等片刻让更新完成
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 5. 再次查询验证
    auto f2 = pool.asyncQueryFuture("SELECT * FROM users");
    QueryResult all = f2.get();
    std::cout << "Total users after insert: " << all.size() << std::endl;

    // 6. 并发测试：同时发起多个查询
    std::cout << "\n=== Concurrent queries ===" << std::endl;
    std::vector<std::future<QueryResult>> futures;
    for (int i = 0; i < 8; ++i) {
        futures.push_back(
            pool.asyncQueryFuture("SELECT SLEEP(0.1), id FROM users LIMIT 1")
        );
    }
    for (auto& f : futures) {
        f.get();  // 等待全部完成
    }
    std::cout << "All concurrent queries done." << std::endl;

    // 7. 关闭连接池
    pool.shutdown();
    return 0;
}