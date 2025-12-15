#pragma once
#include "EntityTypes.h"

namespace monster_ecs {

    struct CTransform {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct CStats {
        int maxHp = 50;
        int hp = 50;
        int atk = 7;
        int def = 0;
    };

    struct CMonsterTag {
        int monsterType = 0;
    };

    struct CSpawnInfo {
        float spawnX = 0.0f;
        float spawnY = 0.0f;
        float respawnDelay;   // 몇 초 후 리스폰할지
        bool  pendingRespawn; // 리스폰 대기 중인지
        float respawnTimer;   // 누적 시간
        float deadTimer = 0.0f;
    };

    struct CAI {
        enum class State : INT32 {
            Idle = 0,
            Patrol = 1,
            Chase = 2,
            Attack = 3,
			Dead = 4,
        };
        State state = State::Idle;
        std::uint64_t targetId = 0;
        float thinkCooldown = 0.0f;

        float attackCooldown{ 1.0f };  // 공격 주기 1초
        float attackTimer{ 0.0f };  // 마지막 공격 시간
    };

    struct CPrefabName {
        std::string name;
    };


} // namespace monster_ecs
