#include "storage/redis/redisUserCache.h"
#include <hiredis.h>
#include <cstdio>
#include <cstdlib>

namespace storage::redis {

    std::string KeyRt(std::uint64_t uid) { return "u:" + std::to_string(uid) + ":rt"; }
    std::string KeyInv(std::uint64_t uid) { return "u:" + std::to_string(uid) + ":inv"; }

    static bool parse_float(redisReply* r, float& out) {
        if (!r) return false;
        if (r->type == REDIS_REPLY_STRING) { out = std::strtof(r->str, nullptr); return true; }
        if (r->type == REDIS_REPLY_INTEGER) { out = (float)r->integer; return true; }
        return false;
    }
    static bool parse_int(redisReply* r, int& out) {
        if (!r) return false;
        if (r->type == REDIS_REPLY_INTEGER) { out = (int)r->integer; return true; }
        if (r->type == REDIS_REPLY_STRING) { out = std::atoi(r->str); return true; }
        return false;
    }

    bool FetchUsers(redisContext* c,
        const std::vector<std::uint64_t>& uids,
        std::vector<UserSnapshot>& out)
    {
        out.clear();
        if (!c) return false;
        if (uids.empty()) return true;

        // 1) 파이프라인: 각 uid마다 HMGET 1개 + GET 1개 = 2개 커맨드
        for (auto uid : uids) {
            const auto krt = KeyRt(uid);
            const auto kinv = KeyInv(uid);

            // HMGET key x z hp sp  -> 배열 4개
            redisAppendCommand(c, "HMGET %s x z hp sp", krt.c_str());
            // GET inv_json
            redisAppendCommand(c, "GET %s", kinv.c_str());
        }

        // 2) 응답 수집: 같은 순서로 2개씩 꺼내기
        out.reserve(uids.size());

        for (auto uid : uids) {
            redisReply* r1 = nullptr;
            redisReply* r2 = nullptr;

            if (redisGetReply(c, (void**)&r1) != REDIS_OK) { if (r1) freeReplyObject(r1); return false; }
            if (redisGetReply(c, (void**)&r2) != REDIS_OK) { if (r1) freeReplyObject(r1); if (r2) freeReplyObject(r2); return false; }

            // HMGET reply
            UserSnapshot s;
            s.uid = uid;

            bool ok = false;
            if (r1 && r1->type == REDIS_REPLY_ARRAY && r1->elements == 4) {
                float x = 0.f, z = 0.f; int hp = 0, sp = 0;
                bool okx = parse_float(r1->element[0], x);
                bool okz = parse_float(r1->element[1], z);
                bool okhp = parse_int(r1->element[2], hp);
                bool oksp = parse_int(r1->element[3], sp);
                ok = okx && okz && okhp && oksp;
                if (ok) { s.x = x; s.z = z; s.hp = hp; s.sp = sp; }
            }

            // GET inv reply (없으면 NIL 가능)
            if (r2) {
                if (r2->type == REDIS_REPLY_STRING && r2->str) s.inv_json.assign(r2->str, r2->len);
                else s.inv_json.clear();
            }

            if (r1) freeReplyObject(r1);
            if (r2) freeReplyObject(r2);

            if (ok) out.push_back(std::move(s)); // rt가 유효한 uid만 업서트
        }

        return true;
    }

} // namespace storage::redis
