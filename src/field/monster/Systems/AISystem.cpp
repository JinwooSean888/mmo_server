#include "AISystem.h"
#include <cmath>
#include <cstdint>

namespace monster_ecs {

    void AISystem::update(float dt, MonsterWorld& ecs, MonsterEnvironment& env)
    {
        for (Entity e : ecs.monsters) {
            auto& st = ecs.stats.get(e);
            auto& ai = ecs.aiComp.get(e);
            auto& tr = ecs.transform.get(e);
            auto& ty = ecs.monsterTag.get(e);
            auto oldState = ai.state;
            const bool isArcher = (ty.monsterType == 1);
            if (!ai.netSynced) {
                // 초기 상태가 뭐든, 현재 ai.state 기준으로 한번은 보내자
                env.broadcastAiState(e, ai.state);
                env.broadcastMonsterStat(e, st.hp, st.maxHp, st.sp, st.maxSp);
                // Idle/Attack/Dead면 무조건 정지 패킷도 한번 보냄
                if (ai.state == CAI::State::Idle || ai.state == CAI::State::Attack || ai.state == CAI::State::Dead) {
                    stop_move(e, ai, env); // set_monster_move(0,0,0) 포함
                }
                else {                                       
                    env.set_monster_move(e, ai.moveDirX, ai.moveDirY, patrol_speed(isArcher));
                }

                ai.netSynced = true;
            }

            // 스탯 dirty 전송
            if (st.dirty) {
                env.broadcastMonsterStat(e, st.hp, st.maxHp, st.sp, st.maxSp);
                st.dirty = false;
            }

            // 죽음 처리
            if (st.hp <= 0) {
                ai.state = CAI::State::Dead;
                ai.targetId = 0;
                stop_move(e, ai, env);
                if (ai.state != oldState)
                    env.broadcastAiState(e, ai.state);
                continue;
            }

            // think tick
            ai.thinkCooldown -= dt;
            if (ai.thinkCooldown > 0.f) continue;
            ai.thinkCooldown = 0.3f;

      

            float sightRange = isArcher ? 18.0f : 11.0f;
            float attackRange = isArcher ? 17.0f : 1.0f;
            float keepAttackRange = isArcher ? 12.0f : 1.5f;

            // ----------------------------
            // 타겟 고정: 기존 타겟 우선, 실패 시 재탐색
            // ----------------------------
            uint64_t target = ai.targetId;

            float px = 0.f, py = 0.f;
            bool hasTargetPos = false;

            if (target) {
                hasTargetPos = env.getPlayerPosition(target, px, py);
                if (!hasTargetPos) {
                    ai.targetId = 0;
                    target = 0;
                }
            }
            if (!target) {
                target = env.findClosestPlayer(tr.x, tr.y, sightRange);
                ai.targetId = target;
                if (target) {
                    hasTargetPos = env.getPlayerPosition(target, px, py);
                    if (!hasTargetPos) {
                        ai.targetId = 0;
                        target = 0;
                    }
                }
            }
            if (target && hasTargetPos) {
                // 추천: sightRange의 1.4배에서 추격 포기
                const float disengageMul = 1.4f;
                const float disengageRange = sightRange * disengageMul;
                const float disengageRangeSq = disengageRange * disengageRange;

                float ddx = px - tr.x;
                float ddy = py - tr.y;
                float distSq = ddx * ddx + ddy * ddy;

                // 너무 멀어지면 타겟 해제 + 로밍 복귀
                if (distSq > disengageRangeSq) {
                    ai.targetId = 0;
                    target = 0;

                    ai.state = CAI::State::Idle;
                    stop_move(e, ai, env);

                    // 다음 틱에 7:3 로밍 재추첨
                    ai.idlePatrolTimer = 0.f;

                    if (ai.state != oldState)
                        env.broadcastAiState(e, ai.state);

                    continue;
                }
            }
            // ----------------------------
            // 1) lowHP면 Return(도망) 우선 + 일정 거리 도망 후 종료
            // ----------------------------
            const float lowHpRatio = 0.25f;
            const bool lowHp = (st.maxHp > 0) && (float(st.hp) / float(st.maxHp) <= lowHpRatio);

            if (lowHp) {
                ai.state = CAI::State::Return;

                // 최소 도망 유지시간
                const float fleeMinTime = 1.2f;
                if (ai.idlePatrolTimer <= 0.f) ai.idlePatrolTimer = fleeMinTime;
                else ai.idlePatrolTimer = std::max(0.f, ai.idlePatrolTimer - ai.thinkCooldown);

                // 충분히 멀어지면 도망 종료 -> Idle
                if (target && hasTargetPos) {
                    float dx = px - tr.x;
                    float dy = py - tr.y;
                    float distSq = dx * dx + dy * dy;

                    const float fleeEndDist = isArcher ? 22.0f : 16.0f;
                    const float fleeEndDistSq = fleeEndDist * fleeEndDist;

                    if (ai.idlePatrolTimer <= 0.f && distSq >= fleeEndDistSq) {
                        ai.targetId = 0;
                        ai.state = CAI::State::Idle;
                        stop_move(e, ai, env);
                        ai.idlePatrolTimer = 0.f; // 다음 틱에 로밍 재추첨
                        if (ai.state != oldState)
                            env.broadcastAiState(e, ai.state);
                        continue;
                    }
                }

                // 도망 방향
                float dirX = ai.moveDirX;
                float dirY = ai.moveDirY;

                if (target && hasTargetPos) {
                    float dx = px - tr.x;
                    float dy = py - tr.y;
                    float d2 = dx * dx + dy * dy;
                    if (d2 > 1e-6f) {
                        float inv = 1.f / std::sqrt(d2);
                        dirX = -dx * inv;
                        dirY = -dy * inv;
                    }
                }
                else {
                    float d2 = dirX * dirX + dirY * dirY;
                    if (d2 < 1e-6f) {
                        float a = frandRange(ai.rng, 0.f, 6.2831853f);
                        dirX = std::cos(a);
                        dirY = std::sin(a);
                    }
                }

                ai.moveDirX = dirX;
                ai.moveDirY = dirY;
                ai.moveSpeed = flee_speed(isArcher);

                env.set_monster_move(e, ai.moveDirX, ai.moveDirY, ai.moveSpeed);

                if (ai.state != oldState)
                    env.broadcastAiState(e, ai.state);

                continue;
            }

            // ----------------------------
            // 2) 타겟 없으면: Patrol/Idle 7:3 로밍
            // ----------------------------
            if (!target) {
                if (ai.idlePatrolTimer <= 0.f) {
                    float r = frand01(ai.rng);
                    bool pickPatrol = (r < 0.7f);

                    if (pickPatrol) {
                        ai.state = CAI::State::Patrol;
                        ai.idlePatrolTimer = frandRange(ai.rng, 1.5f, 3.5f);

                        float a = frandRange(ai.rng, 0.f, 6.2831853f);
                        ai.moveDirX = std::cos(a);
                        ai.moveDirY = std::sin(a);
                        ai.moveSpeed = patrol_speed(isArcher);

                        env.set_monster_move(e, ai.moveDirX, ai.moveDirY, ai.moveSpeed);
                    }
                    else {
                        ai.state = CAI::State::Idle;
                        ai.idlePatrolTimer = frandRange(ai.rng, 0.8f, 2.0f);
                        stop_move(e, ai, env);
                    }
                }
                else {
                    ai.idlePatrolTimer = std::max(0.f, ai.idlePatrolTimer - ai.thinkCooldown);

                    if (ai.state == CAI::State::Idle) stop_move(e, ai, env);
                    else env.set_monster_move(e, ai.moveDirX, ai.moveDirY, ai.moveSpeed);
                }

                if (ai.state != oldState)
                    env.broadcastAiState(e, ai.state);

                continue;
            }

            // ----------------------------
            // 3) 타겟 있으면: 전투 로직
            // ----------------------------
            if (!hasTargetPos) {
                ai.targetId = 0;
                ai.state = CAI::State::Idle;
                stop_move(e, ai, env);
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

                if (distSq < desiredMinSq) {
                    ai.state = CAI::State::Return;

                    if (distSq > 1e-6f) {
                        float inv = 1.f / std::sqrt(distSq);
                        ai.moveDirX = -dx * inv;
                        ai.moveDirY = -dy * inv;
                    }
                    ai.moveSpeed = flee_speed(true);

                    env.set_monster_move(e, ai.moveDirX, ai.moveDirY, ai.moveSpeed);
                }
                else if (distSq <= attackRangeSq) {
                    ai.state = CAI::State::Attack;
                    stop_move(e, ai, env);
                }
                else {
                    ai.state = CAI::State::Chase;
                    ai.moveSpeed = chase_speed(true); // 방향은 MovementSystem이 타겟 보고 계산
                    env.set_monster_move(e, ai.moveDirX, ai.moveDirY, ai.moveSpeed);
                }
            }
            else {
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

                if (ai.state == CAI::State::Attack) {
                    stop_move(e, ai, env);
                }
                else {
                    ai.moveSpeed = chase_speed(false);
                    env.set_monster_move(e, ai.moveDirX, ai.moveDirY, ai.moveSpeed);
                }
            }

            if (ai.state != oldState)
                env.broadcastAiState(e, ai.state);
        }
    }
} // namespace monster_ecs
