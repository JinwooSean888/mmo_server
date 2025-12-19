#pragma once
#include <vector>
#include "ComponentStorage.h"
#include "Components.h"
#include "core_types.h"
#include "MonsterEnvironment.h"

namespace monster_ecs {

    class MonsterEnvironment;
    class SpawnSystem;
    class AISystem;
    class MovementSystem;
    class CombatSystem;

    using Entity = std::uint64_t;   // ?? databaseid를 그대로 Entity로 사용

    class MonsterWorld {
    public:
        MonsterWorld();

        Entity create_monster(INT64 databaseid, float x, float y, const std::string& prefab, int monsterType);

        void kill_monster(Entity e);
        void update(float dt, MonsterEnvironment& env);
        bool player_attack_monster(uint64_t pid, uint64_t mid, MonsterEnvironment& env);

        // ================= Components =================
        ComponentStorage<CTransform>  transform;
        ComponentStorage<CStats>      stats;
        ComponentStorage<CMonsterTag> monsterTag;
        ComponentStorage<CSpawnInfo>  spawnInfo;
        ComponentStorage<CAI>         aiComp;
        ComponentStorage<CPrefabName> prefabNameComp;

        std::vector<Entity> monsters;

    private:
        SpawnSystem* spawnSys_;
        AISystem* aiSys_;
        MovementSystem* moveSys_;
        CombatSystem* combatSys_;
    };

} // namespace monster_ecs
