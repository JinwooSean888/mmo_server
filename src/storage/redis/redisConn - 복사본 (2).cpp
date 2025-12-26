#include "hiredis.h"
#include <iostream>

void test_redis_ping()
{
    redisContext* c = redisConnect("127.0.0.1", 6379);
    if (!c || c->err) {
        std::cout << "[Redis] connect failed: "
            << (c ? c->errstr : "alloc failed") << "\n";
        if (c) redisFree(c);
        return;
    }

    redisReply* r = (redisReply*)redisCommand(c, "PING");
    if (r) {
        std::cout << "[Redis] PING => " << (r->str ? r->str : "(null)") << "\n";
        freeReplyObject(r);
    }

    redisFree(c);
}