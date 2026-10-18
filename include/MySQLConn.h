#ifndef MYSQL_CONN_H
#define MYSQL_CONN_H

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <chrono>

// 查询结果：每行是一个字符串向量
using QueryResult = std::vector<std::vector<std::string>>;

class MySQLConn {
public:
    MySQLConn();
    ~MySQLConn();

    // 禁止拷贝，允许移动
    MySQLConn(const MySQLConn&) = delete;
    MySQLConn& operator=(const MySQLConn&) = delete;
    MySQLConn(MySQLConn&& other) noexcept;
    MySQLConn& operator=(MySQLConn&& other) noexcept;

    // 连接数据库
    bool connect(const std::string& host,
                 const std::string& user,
                 const std::string& password,
                 const std::string& dbname,
                 int port = 3306);

    // 执行查询，返回结果集
    QueryResult query(const std::string& sql);

    // 执行更新（INSERT/UPDATE/DELETE），返回受影响行数
    int update(const std::string& sql);

    // 连接是否有效（发送 SELECT 1 探测）
    bool isAlive();

    // 刷新连接（重连）
    bool refresh();

    // 获取原始 MYSQL* 指针
    MYSQL* raw() { return conn_; }

    // 获取最后活跃时间（用于空闲超时回收）
    std::chrono::steady_clock::time_point lastActiveTime() const {
        return last_active_;
    }

private:
    MYSQL* conn_;
    std::string host_, user_, password_, dbname_;
    int port_;
    std::chrono::steady_clock::time_point last_active_;
};

#endif // MYSQL_CONN_H