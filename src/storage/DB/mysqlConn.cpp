#include "storage/DB/mysqlConn.h"


namespace storage::sql {

    MySqlConn::MySqlConn() {
        mysql_ = mysql_init(nullptr);
    }

    MySqlConn::~MySqlConn() {
        close();
    }

    bool MySqlConn::connect(const char* host, int port,
        const char* user, const char* pass,
        const char* db, const char* charset)
    {
        if (!mysql_) return false;

        mysql_options(mysql_, MYSQL_SET_CHARSET_NAME, charset);

        MYSQL* r = mysql_real_connect(mysql_, host, user, pass, db, port, nullptr, 0);
        return r != nullptr;
    }

    void MySqlConn::close() {
        if (mysql_) {
            mysql_close(mysql_);
            mysql_ = nullptr;
        }
    }

} // namespace storage::sql
