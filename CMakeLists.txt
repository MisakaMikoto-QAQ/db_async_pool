cmake_minimum_required(VERSION 3.10)
project(DbAsyncPool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 手动指定 MySQL 头文件和库（Ubuntu 下 libmysqlclient-dev 安装后路径固定）
set(MYSQL_INCLUDE_DIRS /usr/include/mysql)
set(MYSQL_LIBRARIES mysqlclient)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${MYSQL_INCLUDE_DIRS}
)

# 自动收集 src 下所有 .cpp 文件
file(GLOB SOURCES "src/*.cpp")

add_executable(db_async_pool ${SOURCES})

target_link_libraries(db_async_pool
    PRIVATE
    ${MYSQL_LIBRARIES}
    pthread
)