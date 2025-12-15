#include "MovementSystem.h"
#include "../MonsterWorld.h"
#include "../Components.h"
#include <cmath>

namespace monster_ecs {

    void MovementSystem::update(float dt, MonsterWorld& ecs, MonsterEnvironmentApi& env)
    {
        const float speed = 3.0f;

        for (Entity e : ecs.monsters) {
            auto& ai = ecs.aiComp.get(e);
            if (ai.state != CAI::State::Chase) continue;

            std::cout << "[Move] monsters=" << ecs.monsters.size() << "\n";
            
            std::cout << "[Move] state=" << (int)ai.state << " target=" << ai.targetId << "\n";
            auto& tr = ecs.transform.get(e);

            float px, py;
            if (!env.getPlayerPosition(ai.targetId, px, py)) continue;

            float dx = px - tr.x, dy = py - tr.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len <= 0.001f) continue;

            dx /= len; dy /= len;
            tr.x += dx * speed * dt;
            tr.y += dy * speed * dt;

            if (env.moveInAoi)
                env.moveInAoi(e, tr.x, tr.y);

        }
    }

} // namespace monster_ecs
