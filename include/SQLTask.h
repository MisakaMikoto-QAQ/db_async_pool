#ifndef SQL_TASK_H
#define SQL_TASK_H

#include "MySQLConn.h"
#include <string>
#include <functional>
#include <future>
#include <memory>

// 异步查询的回调函数类型
using QueryCallback = std::function<void(QueryResult)>;

// 任务类型：查询 或 更新
enum class TaskType { QUERY, UPDATE };

class SQLTask {
public:
    // 纯回调风格的查询任务
    SQLTask(TaskType type, std::string sql, QueryCallback cb)
        : type_(type), sql_(std::move(sql)), callback_(std::move(cb)) {}

    // 支持同步等待的查询任务
    SQLTask(TaskType type, std::string sql,
            std::shared_ptr<std::promise<QueryResult>> promise)
        : type_(type), sql_(std::move(sql)), promise_(std::move(promise)) {}

    // 执行任务（由工作线程调用）
    void execute(MySQLConn* conn) {
        if (type_ == TaskType::QUERY) {
            QueryResult result = conn->query(sql_);
            if (callback_) {
                callback_(std::move(result));
            } else if (promise_) {
                promise_->set_value(std::move(result));
            }
        } else {
            int affected = conn->update(sql_);
            if (callback_) {
                // 更新操作的回调接收空结果，受影响行数通过其他方式获取
                callback_(QueryResult{});
            } else if (promise_) {
                promise_->set_value(QueryResult{
                    {std::to_string(affected)}
                });
            }
        }
    }

    TaskType type() const { return type_; }
    const std::string& sql() const { return sql_; }

private:
    TaskType type_;
    std::string sql_;
    QueryCallback callback_;
    std::shared_ptr<std::promise<QueryResult>> promise_;
};

#endif // SQL_TASK_H