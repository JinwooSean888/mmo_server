#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct redisContext;

namespace storage::redis {

    struct UserSnapshot {
        std::uint64_t uid = 0;
        float x = 0.f;
        float z = 0.f;
        int hp = 0;
        int sp = 0;
        std::string inv_json; // 없으면 ""로
    };

    // u:{uid}:rt 에서 x,z,hp,sp 읽고 u:{uid}:inv 에서 JSON 읽기
    // 성공한 uid만 out에 들어감(키 없거나 파싱 실패는 스킵)
    bool FetchUsers(redisContext* c,
        const std::vector<std::uint64_t>& uids,
        std::vector<UserSnapshot>& out);

    std::string KeyRt(std::uint64_t uid);
    std::string KeyInv(std::uint64_t uid);

} // namespace storage::redis
