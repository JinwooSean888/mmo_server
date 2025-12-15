#include "MonsterWorld.h"
#include "MonsterEnvironment.h"
#include "Systems/SpawnSystem.h"
#include "Systems/AISystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/CombatSystem.h"

namespace monster_ecs {

    MonsterWorld::MonsterWorld()
    {
        spawnSys_ = new SpawnSystem();
        aiSys_ = new AISystem();
        moveSys_ = new MovementSystem();
        combatSys_ = new CombatSystem();
    }

    Entity MonsterWorld::create_monster(INT64 databaseid,
        float x, float y,
        const std::string& prefab)
    {
        Entity e = static_cast<Entity>(databaseid);
        monsters.push_back(e);

        transform.add(e, { x, y });
        stats.add(e, {});                // 기본 스탯
        monsterTag.add(e, {});           // 타입 필요하면 나중에 세팅
        spawnInfo.add(e, { x, y });       // spawn 위치
        aiComp.add(e, {});               // 기본 Idle
        prefabNameComp.add(e, { prefab });// ?? 핵심: 프리팹 이름

        return e;
    }

    void MonsterWorld::kill_monster(Entity e)
    {
        stats.get(e).hp = 0;
        aiComp.get(e).state = CAI::State::Dead;
        spawnInfo.get(e).deadTimer = 0.f;
    }

    void MonsterWorld::update(float dt, MonsterEnvironmentApi& env)
    {
        if (spawnSys_)
            spawnSys_->update(dt, *this, env);

        if (aiSys_)
            aiSys_->update(dt, *this, env);

        if (moveSys_)
            moveSys_->update(dt, *this, env);

        if (combatSys_)
            combatSys_->update(dt, *this, env);
    }

    bool MonsterWorld::player_attack_monster(uint64_t pid, uint64_t mid, MonsterEnvironmentApi& env)
    {
        for (auto e : monsters)
        {
            if (e != mid) continue;

            auto& st = stats.get(e);
            st.hp -= 10;

            if (st.hp <= 0)
            {
                st.hp = 0;

                // 죽음 처리
                kill_monster(e);

                // ? 리스폰 대기 ON (SpawnSystem이 보려면 필요)
                auto& sp = spawnInfo.get(e);
                sp.pendingRespawn = true;
                sp.respawnTimer = 0.0f;

                // ? AOI에서 제거 (클라에 Leave 나가서 Destroy됨)
                if (env.removeFromAoi)
                    env.removeFromAoi(e);

                return true; // Dead
            }

            return false;
        }
        return false;
    }


} // namespace monster_ecs