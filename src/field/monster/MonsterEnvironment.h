#pragma once
#include <functional>

namespace monster_ecs {

    struct MonsterEnvironmentApi {
        std::function<std::uint64_t(float, float, float)> findClosestPlayer;
        std::function<bool(std::uint64_t, float&, float&)> getPlayerPosition;
        std::function<void(std::uint64_t, std::uint64_t, int, int)> broadcastCombat;
        std::function<void(std::uint64_t, float, float)> moveInAoi;

        //리스폰 추가 삭제용
        std::function<void(uint64_t mid, float x, float y)> spawnInAoi;
        std::function<void(uint64_t mid)>                  removeFromAoi;
    };

} // namespace monster_ecs
