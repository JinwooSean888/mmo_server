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

            // ----- 타입별 파라미터 설정 -----
            bool isArcher = (ty.monsterType == 1);

            // 근접 기본값
            float sightRange = 10.0f;
            float attackRange = 1.0f;
            float keepAttackRange = 1.5f;
            float fleeRange = 0.0f;  // 근접은 안 씀

            if (isArcher) {
                sightRange = 18.0f;  // 멀리서도 인지
                attackRange = 17.0f;  // 이 정도 거리에서 쏘기
                keepAttackRange = 12.0f;  // 좀 멀어져도 계속 사격
                fleeRange = 10.0f;   // 이 안으로 들어오면 도망
            }

            uint64_t target = env.findClosestPlayer(tr.x, tr.y, sightRange);
            ai.targetId = target;

            // -------- 타겟 없음: Idle <-> Patrol 왕복 --------
            if (!target) {
                // 타이머 증가
                ai.idlePatrolTimer += dt;

                // 원하는 시간 설정
                constexpr float idleDuration = 2.0f; // 가만히 서있는 시간
                constexpr float patrolDuration = 4.0f; // 돌아다니는 시간

                // Idle 상태일 때 일정 시간 지나면 Patrol로
                if (ai.state == CAI::State::Idle) {
                    if (ai.idlePatrolTimer >= idleDuration) {
                        ai.idlePatrolTimer = 0.f;
                        ai.state = CAI::State::Patrol;

                        // 선택: 여기서 순찰 방향/속도 설정
                        // 예: 랜덤 방향으로 천천히 걷기
                        float dirX, dirY;
                        env.pick_random_walk_dir(tr.x, tr.y, dirX, dirY); // 네가 구현
                        env.set_monster_move(e, dirX, dirY, 2.0f);        // (엔티티, 방향, 속도)
                    }
                }
                // Patrol 상태일 때 일정 시간 지나면 Idle로
                else if (ai.state == CAI::State::Patrol) {
                    if (ai.idlePatrolTimer >= patrolDuration) {
                        ai.idlePatrolTimer = 0.f;
                        ai.state = CAI::State::Idle;

                        // 순찰 멈추기
                        env.set_monster_move(e, 0.f, 0.f, 0.f);
                    }
                }
                // 그 외 상태(Chase/Attack 등)에서 타겟이 사라지면 Idle로 리셋
                else {
                    ai.state = CAI::State::Idle;
                    ai.idlePatrolTimer = 0.f;
                    env.set_monster_move(e, 0.f, 0.f, 0.f);
                }

                if (ai.state != oldState) {
                    env.broadcastAiState(e, ai.state);
                }
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
            float fleeRangeSq = fleeRange * fleeRange;

            // ----- 타입별 상태 결정 -----
            if (isArcher) {
                // 궁수용 선호 거리 범위
                float desiredMin = attackRange * 0.7f;   // 이 거리 밑이면 너무 가까움 → 도망
                float desiredMinSq = desiredMin * desiredMin;

                float attackRangeSq = attackRange * attackRange;

                float fleeSpeed = 13.0f;  // 도망 속도 (빠르게)
                float chaseSpeed = 10.0f;  // 쫓아가는 속도 (도망보다 느리게)

                // ----- 너무 가까움: 도망 (Return) -----
                if (distSq < desiredMinSq) {
                    ai.state = CAI::State::Return;

                    if (distSq > 1e-4f) {
                        float invLen = 1.0f / std::sqrt(distSq);

                        // (플레이어 - 몬스터)의 반대 방향으로 도망
                        float awayX = -dx * invLen;
                        float awayY = -dy * invLen;

                        float stepX = awayX * fleeSpeed * dt;
                        float stepY = awayY * fleeSpeed * dt;

                        float newX = tr.x + stepX;
                        float newY = tr.y + stepY;

                        tr.x = newX;
                        tr.y = newY;

                        uint64_t mid = static_cast<uint64_t>(e);
                        env.moveInAoi(mid, newX, newY);
                    }
                    else {
                        uint64_t mid = static_cast<uint64_t>(e);
                        env.moveInAoi(mid, tr.x, tr.y);
                    }
                }
                // ----- 적당한 거리: 제자리에서 쏘기 -----
                else if (distSq <= attackRangeSq) {
                    ai.state = CAI::State::Attack;

                    // 공격은 제자리에서: 이동 안 함, 단 AOI는 최신 위치 유지
                    uint64_t mid = static_cast<uint64_t>(e);
                    env.moveInAoi(mid, tr.x, tr.y);
                }
                // ----- 너무 멀다: 추격해서 다시 사거리 맞추기 -----
                else {
                    ai.state = CAI::State::Chase;

                    if (distSq > 1e-4f) {
                        float invLen = 1.0f / std::sqrt(distSq);

                        float toX = dx * invLen;
                        float toY = dy * invLen;

                        float stepX = toX * chaseSpeed * dt;
                        float stepY = toY * chaseSpeed * dt;

                        float newX = tr.x + stepX;
                        float newY = tr.y + stepY;

                        tr.x = newX;
                        tr.y = newY;

                        uint64_t mid = static_cast<uint64_t>(e);
                        env.moveInAoi(mid, newX, newY);
                    }
                }
            }
            else {
                // === 기존 근접 몬스터 로직은 그대로 유지 ===
                if (ai.state == CAI::State::Attack) {
                    if (distSq > keepAttackRangeSq) {
                        ai.state = CAI::State::Chase;
                    }
                }
                else {
                    if (distSq <= attackRangeSq) {
                        ai.state = CAI::State::Attack;
                    }
                    else {
                        ai.state = CAI::State::Chase;
                    }
                }
            }

            if (ai.state != oldState) {
                env.broadcastAiState(e, ai.state);
            }
        }
    }



} // namespace monster_ecs
