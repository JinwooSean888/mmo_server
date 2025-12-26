#pragma once
#include <string>
#include <mysql.h>

namespace storage::sql {

    class MySqlConn {
    public:
        MySqlConn();
        ~MySqlConn();

        MySqlConn(const MySqlConn&) = delete;
        MySqlConn& operator=(const MySqlConn&) = delete;

        bool connect(const char* host, int port,
            const char* user, const char* pass,
            const char* db, const char* charset = "utf8mb4");

        void close();
        MYSQL* raw() const { return mysql_; }

    private:
        MYSQL* mysql_{ nullptr };
    };

} // namespace storage::sql
