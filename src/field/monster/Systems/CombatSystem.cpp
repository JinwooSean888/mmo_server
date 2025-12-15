#include "CombatSystem.h"
#include "../MonsterWorld.h"
#include "../Components.h"

namespace monster_ecs {

    void CombatSystem::update(float dt, MonsterWorld& ecs, MonsterEnvironmentApi& env)
    {
        for (Entity e : ecs.monsters) {
            auto& ai = ecs.aiComp.get(e);

            if (ai.state != CAI::State::Attack) {
                // 공격 상태가 아니면 타이머 초기화
                ai.attackTimer = 0.0f;
                continue;
            }

            // 공격 쿨타임 누적
            ai.attackTimer += dt;
            if (ai.attackTimer < ai.attackCooldown) {
                // 아직 쿨 안 찼으면 패스
                continue;
            }

            float px, py;
            if (!env.getPlayerPosition(ai.targetId, px, py))
                continue;

            auto& st = ecs.stats.get(e);
            int damage = st.atk;

            // HP 깎는 건 env.broadcastCombat 안에서 처리
            env.broadcastCombat(e, ai.targetId, damage, 0);

            // 쿨타임 리셋
            ai.attackTimer = 0.0f;

            // 굳이 Chase로 돌리지 않고 Attack 유지해도 됨
            // 거리가 멀어지면 AISystem 쪽에서 Attack -> Chase 로 바꾸는 편이 깔끔
            // ai.state = CAI::State::Chase;
        }
    }

} // namespace monster_ecs
