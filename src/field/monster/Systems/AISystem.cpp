#include "AISystem.h"
#include <cmath>

namespace monster_ecs {

    void AISystem::update(float dt, MonsterWorld& ecs, MonsterEnvironment& env)
    {
        for (Entity e : ecs.monsters) {
            auto& st = ecs.stats.get(e);
            auto& ai = ecs.aiComp.get(e);
            auto& tr = ecs.transform.get(e);
            auto& ty = ecs.monsterTag.get(e);
            auto oldState = ai.state;

            if (st.dirty)
			{// 스탯 변경 알림
				env.broadcastMonsterStat(e, st.hp, st.maxHp, st.sp, st.maxSp);
				st.dirty = false;
            }

            if (st.hp <= 0) {
                ai.state = CAI::State::Dead;
                ai.targetId = 0;
                if (ai.state != oldState)
                    env.broadcastAiState(e, ai.state);
                continue;
            }

            ai.thinkCooldown -= dt;
            if (ai.thinkCooldown > 0.f) continue;
            ai.thinkCooldown = 0.3f;

            bool isArcher = (ty.monsterType == 1);

            float sightRange = 10.0f;
            float attackRange = 1.0f;
            float keepAttackRange = 1.5f;
            float fleeRange = 0.0f;

            if (isArcher) {
                sightRange = 18.0f;
                attackRange = 17.0f;
                keepAttackRange = 12.0f;
                fleeRange = 10.0f;
            }

            uint64_t target = env.findClosestPlayer(tr.x, tr.y, sightRange);
            ai.targetId = target;

            // === Patrol 제거 후 target 없으면 Idle만 ===
            if (!target) {
                ai.state = CAI::State::Idle;

                // 이동 중이면 멈춤
                env.set_monster_move(e, 0.f, 0.f, 0.f);

                if (ai.state != oldState)
                    env.broadcastAiState(e, ai.state);

                continue;
            }



            float px, py;
            if (!env.getPlayerPosition(target, px, py)) {
                ai.state = CAI::State::Idle;
                ai.targetId = 0;
                if (ai.state != oldState)
                    env.broadcastAiState(e, ai.state);
                continue;
            }


            float dx = px - tr.x;
            float dy = py - tr.y;
            float distSq = dx * dx + dy * dy;

            float attackRangeSq = attackRange * attackRange;
            float keepAttackRangeSq = keepAttackRange * keepAttackRange;

            if (isArcher) {
                float desiredMin = attackRange * 0.7f;
                float desiredMinSq = desiredMin * desiredMin;
                float fleeSpeed = 13.0f;
                float chaseSpeed = 10.0f;

                // 너무 가까우면 → Return (도망)
                if (distSq < desiredMinSq) {
                    ai.state = CAI::State::Return;

                    if (distSq > 1e-4f) {
                        float invLen = 1.0f / std::sqrt(distSq);
                        float stepX = -dx * invLen * fleeSpeed * dt;
                        float stepY = -dy * invLen * fleeSpeed * dt;

                        tr.x += stepX;
                        tr.y += stepY;

                        env.moveInAoi(e, tr.x, tr.y);
                    }
                }
                // 적당하면 제자리 공격
                else if (distSq <= attackRangeSq) {
                    ai.state = CAI::State::Attack;
                    env.moveInAoi(e, tr.x, tr.y);
                }
                // 멀면 추격
                else {
                    ai.state = CAI::State::Chase;

                    if (distSq > 1e-4f) {
                        float invLen = 1.0f / std::sqrt(distSq);
                        float stepX = dx * invLen * chaseSpeed * dt;
                        float stepY = dy * invLen * chaseSpeed * dt;

                        tr.x += stepX;
                        tr.y += stepY;

                        env.moveInAoi(e, tr.x, tr.y);
                    }
                }
            }
            else {
                // 근접 AI 유지
                if (ai.state == CAI::State::Attack) {
                    if (distSq > keepAttackRangeSq)
                        ai.state = CAI::State::Chase;
                }
                else {
                    if (distSq <= attackRangeSq)
                        ai.state = CAI::State::Attack;
                    else
                        ai.state = CAI::State::Chase;
                }
            }

            if (ai.state != oldState) {
                env.broadcastAiState(e, ai.state);
            }
        }
    }


} // namespace monster_ecs
