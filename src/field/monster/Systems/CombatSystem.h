#pragma once
#include "../MonsterEnvironment.h"

namespace monster_ecs {

    class CombatSystem {
    public:
        void update(float dt, class MonsterWorld& ecs, MonsterEnvironment& env);
    };

} // namespace monster_ecs
