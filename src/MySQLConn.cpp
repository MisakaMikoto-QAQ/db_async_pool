#include "MySQLConn.h"
#include <stdexcept>
#include <cstring>
#include <iostream>

MySQLConn::MySQLConn()
    : conn_(mysql_init(nullptr)), port_(3306),
      last_active_(std::chrono::steady_clock::now()) {
    if (!conn_) {
        throw std::runtime_error("mysql_init failed");
    }
    mysql_set_character_set(conn_, "utf8mb4");
}

MySQLConn::~MySQLConn() {
    if (conn_) {
        mysql_close(conn_);
        conn_ = nullptr;
    }
}

MySQLConn::MySQLConn(MySQLConn&& other) noexcept
    : conn_(other.conn_), host_(std::move(other.host_)),
      user_(std::move(other.user_)), password_(std::move(other.password_)),
      dbname_(std::move(other.dbname_)), port_(other.port_),
      last_active_(other.last_active_) {
    other.conn_ = nullptr;
}

MySQLConn& MySQLConn::operator=(MySQLConn&& other) noexcept {
    if (this != &other) {
        if (conn_) mysql_close(conn_);
        conn_ = other.conn_;
        host_ = std::move(other.host_);
        user_ = std::move(other.user_);
        password_ = std::move(other.password_);
        dbname_ = std::move(other.dbname_);
        port_ = other.port_;
        last_active_ = other.last_active_;
        other.conn_ = nullptr;
    }
    return *this;
}

bool MySQLConn::connect(const std::string& host,
                        const std::string& user,
                        const std::string& password,
                        const std::string& dbname,
                        int port) {
    host_ = host;
    user_ = user;
    password_ = password;
    dbname_ = dbname;
    port_ = port;

    if (!mysql_real_connect(conn_, host_.c_str(), user_.c_str(),
                            password_.c_str(), dbname_.c_str(), port_,
                            nullptr, 0)) {
        std::cerr << "[MySQLConn] connect failed: "
                  << mysql_error(conn_) << std::endl;
        return false;
    }
    last_active_ = std::chrono::steady_clock::now();
    return true;
}

QueryResult MySQLConn::query(const std::string& sql) {
    QueryResult result;
    if (mysql_query(conn_, sql.c_str()) != 0) {
        std::cerr << "[MySQLConn] query failed: "
                  << mysql_error(conn_) << std::endl;
        return result;
    }

    MYSQL_RES* res = mysql_store_result(conn_);
    if (!res) {
        // 可能是非 SELECT 语句
        return result;
    }

    int num_fields = mysql_num_fields(res);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        std::vector<std::string> row_data;
        for (int i = 0; i < num_fields; ++i) {
            row_data.emplace_back(row[i] ? row[i] : "NULL");
        }
        result.push_back(std::move(row_data));
    }

    mysql_free_result(res);
    last_active_ = std::chrono::steady_clock::now();
    return result;
}

int MySQLConn::update(const std::string& sql) {
    if (mysql_query(conn_, sql.c_str()) != 0) {
        std::cerr << "[MySQLConn] update failed: "
                  << mysql_error(conn_) << std::endl;
        return -1;
    }
    last_active_ = std::chrono::steady_clock::now();
    return static_cast<int>(mysql_affected_rows(conn_));
}

bool MySQLConn::isAlive() {
    if (mysql_ping(conn_) != 0) {
        return false;
    }
    last_active_ = std::chrono::steady_clock::now();
    return true;
}

bool MySQLConn::refresh() {
    if (conn_) {
        mysql_close(conn_);
        conn_ = mysql_init(nullptr);
        if (!conn_) return false;
        mysql_set_character_set(conn_, "utf8mb4");
    }
    return connect(host_, user_, password_, dbname_, port_);
}