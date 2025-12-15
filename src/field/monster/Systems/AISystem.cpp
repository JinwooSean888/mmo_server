#include "AISystem.h"
#include <cmath>

namespace monster_ecs {

    void AISystem::update(float dt, MonsterWorld& ecs, MonsterEnvironmentApi& env)
    {
        for (Entity e : ecs.monsters) {
            auto& st = ecs.stats.get(e);
            auto& ai = ecs.aiComp.get(e);
            auto& tr = ecs.transform.get(e);

            if (st.hp <= 0) {
                ai.state = CAI::State::Dead;
                ai.targetId = 0;
                continue;
            }

            ai.thinkCooldown -= dt;
            if (ai.thinkCooldown > 0.f) continue;
            ai.thinkCooldown = 0.3f;

            // 시야 거리 (예: 10m)
            constexpr float sightRange = 10.0f;
            constexpr float attackRange = 1.0f;  // 공격 시작 거리
            constexpr float keepAttackRange = 1.5f; // 공격 유지 거리

            uint64_t target = env.findClosestPlayer(tr.x, tr.y, sightRange);
            ai.targetId = target;

            if (!target) {
                ai.state = CAI::State::Idle;
                continue;
            }

            float px, py;
            if (!env.getPlayerPosition(target, px, py)) {
                ai.state = CAI::State::Idle;
                ai.targetId = 0;
                continue;
            }

            float dx = px - tr.x, dy = py - tr.y;
            float distSq = dx * dx + dy * dy;

            float attackRangeSq = attackRange * attackRange;
            float keepAttackRangeSq = keepAttackRange * keepAttackRange;

            if (ai.state == CAI::State::Attack) {
                // 이미 공격 중일 때는 조금 더 멀어져도 Attack 유지
                if (distSq > keepAttackRangeSq) {
                    ai.state = CAI::State::Chase;
                }
                // else: 그대로 Attack 유지
            }
            else {
                // Chase/Idle 상태일 때는 공격 거리 안으로 들어오면 Attack으로 전환
                if (distSq <= attackRangeSq) {
                    ai.state = CAI::State::Attack;
                }
                else {
                    ai.state = CAI::State::Chase;
                }
            }
        }
    }


} // namespace monster_ecs
