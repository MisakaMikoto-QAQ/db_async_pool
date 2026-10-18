#ifndef MYSQL_CONN_POOL_H
#define MYSQL_CONN_POOL_H

#include "MySQLConn.h"
#include "BlockingQueue.h"
#include "SQLTask.h"
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>

class MySQLConnPool {
public:
    // 获取单例（C++11 magic static，线程安全）
    static MySQLConnPool& instance() {
        static MySQLConnPool pool;
        return pool;
    }

    // 初始化：创建连接和线程
    bool init(const std::string& host, const std::string& user,
              const std::string& password, const std::string& dbname,
              int pool_size = 4, int thread_count = 4, int port = 3306);

    // 异步查询（回调风格）
    void asyncQuery(const std::string& sql, QueryCallback callback);

    // 异步查询（future 风格，可等待结果）
    std::future<QueryResult> asyncQueryFuture(const std::string& sql);

    // 异步更新
    void asyncUpdate(const std::string& sql, QueryCallback callback = nullptr);

    // 关闭连接池
    void shutdown();

    // 获取空闲连接数
    size_t idleCount() const;

    ~MySQLConnPool();

private:
    MySQLConnPool() = default;
    MySQLConnPool(const MySQLConnPool&) = delete;
    MySQLConnPool& operator=(const MySQLConnPool&) = delete;

    // 从空闲队列获取一个连接（阻塞等待）
    MySQLConn* acquireConnection();

    // 归还连接到空闲队列
    void releaseConnection(MySQLConn* conn);

    // 工作线程主循环
    void workerLoop();

    // 连接参数
    std::string host_, user_, password_, dbname_;
    int port_ = 3306;

    // 连接池
    std::vector<std::unique_ptr<MySQLConn>> all_connections_;
    BlockingQueue<MySQLConn*> idle_queue_;
    std::mutex pool_mutex_;
    std::condition_variable pool_cv_;

    // 任务队列
    BlockingQueue<std::shared_ptr<SQLTask>> task_queue_;

    // 工作线程
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};

    int pool_size_ = 4;
    int thread_count_ = 4;
};

#endif // MYSQL_CONN_POOL_H