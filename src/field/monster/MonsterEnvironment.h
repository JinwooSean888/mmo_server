#pragma once
#include <functional>
#include <cstdint>
#include <random>

#include "monster/Components.h"
//#include "MonsterWorld.h"

namespace monster_ecs {

    class MonsterWorld;
    class MonsterEnvironment {

    public:
        std::function<std::uint64_t(float, float, float)> findClosestPlayer;
        std::function<bool(std::uint64_t, float&, float&)> getPlayerPosition;
        std::function<void(std::uint64_t, std::uint64_t, int, int)> broadcastCombat;
        std::function<void(std::uint64_t, float, float)> moveInAoi;
        std::function<void(uint64_t, monster_ecs::CAI::State)>  broadcastAiState;

        std::function<void(uint64_t mid, float x, float y)> spawnInAoi;
        std::function<void(uint64_t mid)>                  removeFromAoi;

        MonsterEnvironment(MonsterWorld& world);

        void pick_random_walk_dir(float x, float y,
            float& outDx, float& outDy);

        void set_monster_move(Entity e,
            float dx, float dy,
            float speed);

    private:
        MonsterWorld& world_;
        std::mt19937 rng_{ std::random_device{}() };
    };

} // namespace monster_ecs
