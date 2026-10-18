#include "MySQLConnPool.h"
#include <iostream>

bool MySQLConnPool::init(const std::string& host, const std::string& user,
                          const std::string& password, const std::string& dbname,
                          int pool_size, int thread_count, int port) {
    host_ = host;
    user_ = user;
    password_ = password;
    dbname_ = dbname;
    port_ = port;
    pool_size_ = pool_size;
    thread_count_ = thread_count;

    // 1. 创建物理连接
    for (int i = 0; i < pool_size_; ++i) {
        auto conn = std::make_unique<MySQLConn>();
        if (!conn->connect(host_, user_, password_, dbname_, port_)) {
            std::cerr << "[Pool] Failed to create connection " << i << std::endl;
            return false;
        }
        idle_queue_.push(conn.get());
        all_connections_.push_back(std::move(conn));
    }
    std::cout << "[Pool] Created " << pool_size_ << " connections." << std::endl;

    // 2. 启动工作线程
    running_ = true;
    for (int i = 0; i < thread_count_; ++i) {
        workers_.emplace_back(&MySQLConnPool::workerLoop, this);
    }
    std::cout << "[Pool] Started " << thread_count_ << " worker threads." << std::endl;
    return true;
}

void MySQLConnPool::workerLoop() {
    while (running_) {
        std::shared_ptr<SQLTask> task;
        // 带超时的等待，便于 shutdown 时退出
        if (!task_queue_.pop_with_timeout(task, 500)) {
            continue;
        }

        // 获取一个空闲连接
        MySQLConn* conn = acquireConnection();
        if (!conn) continue;

        // 执行任务
        try {
            task->execute(conn);
        } catch (const std::exception& e) {
            std::cerr << "[Pool] Task execution error: " << e.what() << std::endl;
        }

        // 归还连接
        releaseConnection(conn);
    }
}

MySQLConn* MySQLConnPool::acquireConnection() {
    MySQLConn* conn = nullptr;
    // 阻塞等待空闲连接
    conn = idle_queue_.pop();
    return conn;
}

void MySQLConnPool::releaseConnection(MySQLConn* conn) {
    if (conn) {
        idle_queue_.push(conn);
    }
}

void MySQLConnPool::asyncQuery(const std::string& sql, QueryCallback callback) {
    auto task = std::make_shared<SQLTask>(TaskType::QUERY, sql, std::move(callback));
    task_queue_.push(task);
}

std::future<QueryResult> MySQLConnPool::asyncQueryFuture(const std::string& sql) {
    auto promise = std::make_shared<std::promise<QueryResult>>();
    auto future = promise->get_future();
    auto task = std::make_shared<SQLTask>(TaskType::QUERY, sql, promise);
    task_queue_.push(task);
    return future;
}

void MySQLConnPool::asyncUpdate(const std::string& sql, QueryCallback callback) {
    auto task = std::make_shared<SQLTask>(TaskType::UPDATE, sql, std::move(callback));
    task_queue_.push(task);
}

void MySQLConnPool::shutdown() {
    if (!running_) return;
    running_ = false;

    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();

    all_connections_.clear();
    std::cout << "[Pool] Shutdown complete." << std::endl;
}

size_t MySQLConnPool::idleCount() const {
    return idle_queue_.size();
}

MySQLConnPool::~MySQLConnPool() {
    shutdown();
}