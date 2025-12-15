// FieldWorker.cpp

#include "fieldWorker.h"
#include "workerManager.h"
#include "net/session.h"
#include "net/sessionManager.h"
#include "field/FieldAoiSystem.h"
#include "field/FieldManager.h"
#include "game/PlayerManager.h"
#include "proto/generated/field_generated.h" // FieldCmd, Vec2 등
#include "proto/generated/game_generated.h" // FieldCmd, Vec2 등

using net::SessionManager;

namespace core {

    // --------------------------------------------------------------------
    // 내부 유틸: FieldWorker 이름 생성
    // --------------------------------------------------------------------
    static std::string make_field_worker_name(int fieldId)
    {
        return "FieldWorker_" + std::to_string(fieldId);
    }
    static bool is_monster_id(std::uint64_t id)
    {
        // 네가 정한 규칙에 맞춰서 수정
        return id >= 1000;
    }

    // --------------------------------------------------------------------
    // 생성자
    // --------------------------------------------------------------------
    FieldWorker::FieldWorker(int fieldId)
        : Worker(make_field_worker_name(fieldId))
        , fieldId_(fieldId)
    {
        init_monster_env();
        aoiSystem_ = std::make_shared<FieldAoiSystem>(
            fieldId_,
            10.0f,
            1,
            [this](std::uint64_t watcherId, const AoiEvent& ev)
            {
                std::cout << "[AOI] watcher=" << watcherId
                    << " subject=" << ev.subjectId
                    << " type=" << (int)ev.type
                    << " pos=" << ev.position.x << "," << ev.position.y
                    << std::endl;

                auto sess = SessionManager::instance().find_by_player_id(watcherId);
                if (!sess)
                    return;


                flatbuffers::FlatBufferBuilder fbb;
                auto pos = field::CreateVec2(fbb, ev.position.x, ev.position.y);

                field::FieldCmdType cmdType = field::FieldCmdType::FieldCmdType_Move;
                switch (ev.type)
                {
                case AoiEvent::Type::Snapshot:
                case AoiEvent::Type::Enter:
                    cmdType = field::FieldCmdType::FieldCmdType_Enter;
                    break;
                case AoiEvent::Type::Leave:
                    cmdType = field::FieldCmdType::FieldCmdType_Leave;
                    break;
                case AoiEvent::Type::Move:
                    cmdType = field::FieldCmdType::FieldCmdType_Move;
                    break;
                }

                // 플레이어/몬스터 구분
                bool isMonster = is_monster_id(ev.subjectId);
                field::EntityType et = field::EntityType::EntityType_Player;
                if (isMonster) {
                    et = field::EntityType::EntityType_Monster;
                }

                // 프리팹 이름 가져오기                
                std::string prefabName = get_prefab_name(ev.subjectId, isMonster);
                auto prefabStr = fbb.CreateString(prefabName);

                // 새 FieldCmd 시그니처에 맞춰 호출
                auto cmd = field::CreateFieldCmd(
                    fbb,
                    cmdType,        // type
                    et,             // entityType
                    ev.subjectId,   // entityId
                    pos,            // pos
                    0,              // dir (서버->클라 방향에선 아직 안 씀)
                    prefabStr       // ★ 새로 추가된 prefab 필드
                );
                fbb.Finish(cmd);

                sess->send_payload(fbb.GetBufferPointer(), fbb.GetSize());

            }
        );


        // Worker::loop()에서 메시지를 꺼낼 때 호출되는 콜백
        set_on_message(
            [this](const NetMessage& msg)
            {
                handle_message(msg);
            }
        );


        // === 몬스터 초기 생성 ===
        if (fieldId == 1000) {
            SpawnMonstersEvenGrid(1000);
            //auto monsterId = MakeDatabaseID(1);
            //monsterWorld_.create_monster(monsterId, 102.0f, 155.0f, "SingleTwoHandSwordTemplate");
            //
            //if (aoiSystem_) {
            //    // 몬스터라서 isPlayer = false
            //    aoiSystem_->add_entity(monsterId, /*isPlayer=*/false, 102.0f, 155.0f);
            //}
            //monsterId = MakeDatabaseID(1);
            //monsterWorld_.create_monster(monsterId, 102.0f, 155.0f, "BowAndArrowTemplate_1");

            //if (aoiSystem_) {
            //    // 몬스터라서 isPlayer = false
            //    aoiSystem_->add_entity(monsterId, /*isPlayer=*/false, 102.0f, 155.0f);
            //}
            //monsterId = MakeDatabaseID(1);
            //monsterWorld_.create_monster(monsterId, 102.0f, 155.0f, "BowAndArrowTemplate_2");

            //if (aoiSystem_) {
            //    // 몬스터라서 isPlayer = false
            //    aoiSystem_->add_entity(monsterId, /*isPlayer=*/false, 102.0f, 155.0f);
            //}
            //monsterId = MakeDatabaseID(1);
            //monsterWorld_.create_monster(monsterId, 102.0f, 155.0f, "DoubleSwordsTemplate_1");

            //if (aoiSystem_) {
            //    // 몬스터라서 isPlayer = false
            //    aoiSystem_->add_entity(monsterId, /*isPlayer=*/false, 102.0f, 155.0f);
            //}
            //monsterId = MakeDatabaseID(1);
            //monsterWorld_.create_monster(monsterId, 102.0f, 155.0f, "DoubleSwordsTemplate_3");

            //if (aoiSystem_) {
            //    // 몬스터라서 isPlayer = false
            //    aoiSystem_->add_entity(monsterId, /*isPlayer=*/false, 102.0f, 155.0f);
            //}
            //monsterId = MakeDatabaseID(1);
            //monsterWorld_.create_monster(monsterId, 102.0f, 155.0f, "MagicWandTemplate_1");

            //if (aoiSystem_) {
            //    // 몬스터라서 isPlayer = false
            //    aoiSystem_->add_entity(monsterId, /*isPlayer=*/false, 102.0f, 155.0f);
            //}
            //monsterId = MakeDatabaseID(1);
            //monsterWorld_.create_monster(monsterId, 102.0f, 155.0f, "MagicWandTemplate_2");

            //if (aoiSystem_) {
            //    // 몬스터라서 isPlayer = false
            //    aoiSystem_->add_entity(monsterId, /*isPlayer=*/false, 102.0f, 155.0f);
            //}
            //monsterId = MakeDatabaseID(1);
            //monsterWorld_.create_monster(monsterId, 102.0f, 155.0f, "NoWeaponTemplate_1");

            //if (aoiSystem_) {
            //    // 몬스터라서 isPlayer = false
            //    aoiSystem_->add_entity(monsterId, /*isPlayer=*/false, 102.0f, 155.0f);
            //}
            //monsterId = MakeDatabaseID(1);
            //monsterWorld_.create_monster(monsterId, 102.0f, 155.0f, "NoWeaponTemplate_2");

            //if (aoiSystem_) {
            //    // 몬스터라서 isPlayer = false
            //    aoiSystem_->add_entity(monsterId, /*isPlayer=*/false, 102.0f, 155.0f);
            //}
            //monsterId = MakeDatabaseID(1);
            //monsterWorld_.create_monster(monsterId, 102.0f, 155.0f, "SingleTwoHandSwordTemplate");

            //if (aoiSystem_) {
            //    // 몬스터라서 isPlayer = false
            //    aoiSystem_->add_entity(monsterId, /*isPlayer=*/false, 102.0f, 155.0f);
            //}
        }
    }

    FieldWorker::~FieldWorker() = default;

    void FieldWorker::handle_message(const NetMessage& msg)
    {
        if (!aoiSystem_)
            return;

        if (msg.type == MessageType::Custom)
        {
            const uint8_t* buf = msg.payload.data();
            auto* cmd = field::GetFieldCmd(buf);
            if (!cmd) return;

            // 이제 Custom 은 "클라의 Move 입력"만 처리
            if (cmd->type() == field::FieldCmdType::FieldCmdType_Move)
            {
                on_client_move_input(*cmd, msg.session);
            }
            return;
        }
        else if (msg.type == MessageType::SkillCmd)
        {
            handle_skill(msg);
        }
    }


    void FieldWorker::on_client_move_input(const field::FieldCmd& cmd, net::Session::Ptr session)
    {
        if (!session) return;

        // 보안: 세션의 playerId 와 패킷의 playerId 가 같은지 확인
        if (session->player_id() != cmd.entityId())
            return;

        if (cmd.type() != field::FieldCmdType::FieldCmdType_Move)
            return;

        auto it = players_.find(cmd.entityId());
        if (it == players_.end())
            return;

        auto* dir = cmd.dir();   // 클라가 보낸 입력 방향 (dir 필드 추가했다고 가정)
        if (!dir)
            return;

        float dx = dir->x();
        float dy = dir->y();
        float len2 = dx * dx + dy * dy;

        auto& mv = it->second->move_state();

		// 무브 입력 시간 갱신
        mv.lastInputTime = worldTime_;

        if (len2 < 1e-4f) {
            mv.moving = false;
            mv.dir = { 0.f, 0.f };
            mv.speed = 0.f;
            return;
        }

        float invLen = 1.0f / std::sqrt(len2);
        dx *= invLen;
        dy *= invLen;

        mv.moving = true;
        mv.dir = { dx, dy };
        mv.speed = 10.0f; // 서버가 결정하는 속도
    }


    // --------------------------------------------------------------------
    // 플레이어 등록/제거
    // --------------------------------------------------------------------
    void FieldWorker::add_player(Player::Ptr player)
    {
        if (!player) return;

        const uint64_t pid = player->id();
        players_[pid] = player;

        // 최초 위치 (네가 쓰는 초기 좌표로 유지)
        Vec2 p = player->pos();   // 이미 (5,5) 로 셋팅되어 있으면 그대로 사용

        std::cout << "[FieldWorker] add_player id=" << pid
            << " pos=" << p.x << "," << p.y << std::endl;

        // 필드 입장 처리 + 스냅샷/브로드캐스트
        on_player_enter_field(player);
    }

    void FieldWorker::remove_player(std::uint64_t playerId)
    {
        if (aoiSystem_) {
            aoiSystem_->remove_entity(playerId);
        }
        players_.erase(playerId);
    }


    // --------------------------------------------------------------------
    // 틱 업데이트: 서버 권한 이동 + AOI 반영
    //  - 이 경로는 "서버 내부 로직" 전용
    // --------------------------------------------------------------------
    void FieldWorker::update_world(float dt)
    {
        if (dt <= 0.0f) return;

        worldTime_ += dt;
  /*      std::cout << "[FW] field=" << fieldId_
            << " monsters=" << monsterWorld_.monsters.size()
            << " world_ptr=" << (void*)&monsterWorld_
            << "\n";*/
        // ----------------------------
        // 플레이어 고정 틱 (20Hz)
        // ----------------------------
        int playerLoops = 0;
        playerAcc_ += dt;
        while (playerAcc_ >= PlayerStep && playerLoops < 5)
        {
            tick_players(PlayerStep);
            playerAcc_ -= PlayerStep;
            playerLoops++;
        }

        // ----------------------------
        // 몬스터 고정 틱 (10Hz)
        // ----------------------------
        int monsterLoops = 0;
        monsterAcc_ += dt;
        while (monsterAcc_ >= MonsterStep && monsterLoops < 3)
        {
            tick_monsters(MonsterStep);
            monsterAcc_ -= MonsterStep;
            monsterLoops++;
        }
    }

    // --------------------------------------------------------------------
    // 맵/충돌 체크 (현재는 스텁)
    // --------------------------------------------------------------------
    bool FieldWorker::is_walkable(const Vec2& from, const Vec2& to) const
    {
        (void)from;
        (void)to;
        // TODO: 맵 데이터, 타일, 높이맵 등과 연동해서 실제 충돌 검사를 구현
        return true;
    }

    void FieldWorker::send_combat_event(
        field::EntityType attackerType,uint64_t attackerId, field::EntityType targetType, uint64_t  targetId, int damage, int remainHp)
    {
        flatbuffers::FlatBufferBuilder fbb;

        auto ev = field::CreateCombatEvent(
            fbb,
            attackerType,
            attackerId,
            targetType,
            targetId,
            damage,
            remainHp
        );
        fbb.Finish(ev);

        // 일단: 피격당한 플레이어 본인에게만 전송 (주변에게도 뿌리고 싶으면 AOI 사용)
        auto sess = net::SessionManager::instance().find_by_player_id(targetId);
        if (!sess)
            return;

        sess->send_payload(fbb.GetBufferPointer(), fbb.GetSize());
    }

    void FieldWorker::monster_spawn_in_aoi(std::uint64_t monsterId, float x, float y)
    {
        if (aoiSystem_) {
            // 몬스터라서 isPlayer = false
            aoiSystem_->add_entity(monsterId, /*isPlayer=*/false, x, y);
        }
    }

    void FieldWorker::monster_remove_from_aoi(std::uint64_t monsterId)
    {
        if (aoiSystem_) {
            aoiSystem_->remove_entity(monsterId);
        }
    }
    void FieldWorker::handle_skill(const NetMessage& msg)
    {
        auto session = msg.session;
        if (!session) return;

        uint64_t pid = session->player_id();

        flatbuffers::Verifier verifier(
            reinterpret_cast<const uint8_t*>(msg.payload.data()),
            msg.payload.size());

        if (!verifier.VerifyBuffer<game::SkillCmd>(nullptr)) {
            std::cout << "[WARN] Invalid SkillCmd" << std::endl;
            return;
        }

        auto skill = flatbuffers::GetRoot<game::SkillCmd>(msg.payload.data());
        if (!skill) {
            std::cout << "[WARN] SkillCmd null" << std::endl;
            return;
        }

        uint64_t targetId = skill->targetId();

        std::cout << "[SKILL] Player " << pid
            << " attacked target=" << targetId
            << " skill=" << static_cast<int>(skill->skill())
            << std::endl;

        // ?? 모든 사망/리스폰/AOI 처리는 MonsterWorld 내부에서
        bool dead = monsterWorld_.player_attack_monster(pid, targetId, env_);

        if (dead)
        {
            std::cout << "[Monster] Dead id=" << targetId << std::endl;
        }
    }

    std::string FieldWorker::get_prefab_name(uint64_t id, bool isMonster)
    {
        if (isMonster)
        {

           if (monsterWorld_.prefabNameComp.has(id))
                 return monsterWorld_.prefabNameComp.get(id).name;
            
            return "";  // 못 찾으면 빈 문자열
        }

        // 플레이어라면
        auto pit = players_.find(id);
        if (pit != players_.end())
            return "Paladin";   // 플레이어는 프리셋으로 Paladin 사용

        return "";
    }

    // 맨 위 네임스페이스 core 안, FieldWorker 메서드들 근처에 추가
    void FieldWorker::send_field_enter(std::uint64_t watcherId,std::uint64_t subjectId,bool isMonster,const Vec2& pos)
    {
        auto sess = net::SessionManager::instance().find_by_player_id(watcherId);
        if (!sess) return;

        flatbuffers::FlatBufferBuilder fbb;

        auto posOffset = field::CreateVec2(fbb, pos.x, pos.y);

        field::EntityType et = isMonster
            ? field::EntityType::EntityType_Monster
            : field::EntityType::EntityType_Player;

        // 몬스터만 prefab 사용, 플레이어는 "" 로 보냄
        std::string prefabName = get_prefab_name(subjectId, isMonster);
        auto prefabStr = fbb.CreateString(prefabName);

        auto cmd = field::CreateFieldCmd(
            fbb,
            field::FieldCmdType::FieldCmdType_Enter, // type
            et,                                      // entityType
            subjectId,                               // entityId
            posOffset,                               // pos
            0,                                       // dir
            prefabStr                                // prefab
        );

        fbb.Finish(cmd);
        sess->send_payload(fbb.GetBufferPointer(), fbb.GetSize());
    }
    // FieldWorker 멤버로 추가
    void FieldWorker::on_player_enter_field(Player::Ptr player)
    {
        if (!player) return;

        const uint64_t pid = player->id();
        Vec2 p = player->pos();   // 예: {5,5}

        // 1) AOI에 등록 (이제부터 Move 이벤트는 AOI 통해서 내려감)
        if (aoiSystem_) {
            aoiSystem_->add_entity(pid, /*isPlayer=*/true, p.x, p.y);
        }

        // 2) 이 플레이어에게 "본인 + 기존 엔티티" 스냅샷 보내기
        // 2-1) 자기 자신
        send_field_enter(pid, pid, /*isMonster=*/false, p);

        // 2-2) 기존 플레이어들
        for (auto& [otherId, other] : players_) {
            if (otherId == pid) continue;
            if (!other) continue;

            Vec2 op = other->pos();
            // 새로 들어온 플레이어 시점에서, 기존 애들을 Enter 로 쫙 내려줌
            send_field_enter(pid, otherId, /*isMonster=*/false, op);
        }

        // 2-3) 기존 몬스터들
        for (auto mId : monsterWorld_.monsters) {
            // 네 ECS 구조에 맞게 위치 얻는 부분만 맞춰줘
            auto& tr = monsterWorld_.transform.get(mId);
            Vec2 mp{ tr.x, tr.y };

            send_field_enter(pid, mId, /*isMonster=*/true, mp);
        }

        // 3) 기존 플레이어들에게 “새로 들어온 애”를 Enter 로 알려주기
        for (auto& [otherId, other] : players_) {
            if (otherId == pid) continue;
            if (!other) continue;

            send_field_enter(otherId, pid, /*isMonster=*/false, p);
        }

        // 마지막으로 디버그 로그
        std::cout << "[FieldWorker] on_player_enter_field pid="
            << pid << " pos=" << p.x << "," << p.y << std::endl;
    }
    // FieldWorker.cpp
    void FieldWorker::init_monster_env()
    {
        env_.findClosestPlayer = [this](float x, float y, float maxDist) -> uint64_t {
            uint64_t closestId = 0;
            float closestDistSq = maxDist * maxDist;

            for (auto& [pid, player] : players_) {
                if (!player) continue;
                Vec2 pos = player->pos();
                float dx = pos.x - x;
                float dy = pos.y - y;
                float distSq = dx * dx + dy * dy;
                if (distSq < closestDistSq) {
                    closestDistSq = distSq;
                    closestId = pid;
                }
            }
            return closestId;
            };

        env_.getPlayerPosition = [this](uint64_t pid, float& outX, float& outY) -> bool {
            auto it = players_.find(pid);
            if (it == players_.end() || !it->second) return false;
            outX = it->second->pos().x;
            outY = it->second->pos().y;
            return true;
            };

        env_.broadcastCombat = [this](uint64_t monsterId, uint64_t playerId, int damage, int /*dummy*/)
            {
                auto it = players_.find(playerId);
                if (it == players_.end() || !it->second) return;

                auto& st = it->second->player_stat();
                st.hp -= damage;
                if (st.hp < 0) st.hp = 0;

                send_combat_event(
                    field::EntityType::EntityType_Monster, monsterId,
                    field::EntityType::EntityType_Player, playerId,
                    damage, st.hp
                );

                if (st.hp == 0) {
                    // TODO: 죽음 처리
                }
            };

        env_.moveInAoi = [this](uint64_t mid, float x, float y) {
            if (aoiSystem_) aoiSystem_->move_entity(mid, x, y);
            };

        env_.spawnInAoi = [this](uint64_t mid, float x, float y) {
            if (!aoiSystem_) return;
            aoiSystem_->remove_entity(mid);
            aoiSystem_->add_entity(mid, /*isPlayer=*/false, x, y);
            };

        env_.removeFromAoi = [this](uint64_t mid) {
            if (aoiSystem_) aoiSystem_->remove_entity(mid);
            };
    }
    void FieldWorker::tick_players(float step)
    {
        for (auto& [pid, player] : players_) {
            if (!player) continue;

            auto& mv = player->move_state();

            if (mv.moving && (worldTime_ - mv.lastInputTime) > 0.5f) {
                mv.moving = false;
                mv.dir = { 0.f, 0.f };
                mv.speed = 0.f;
                continue;
            }

            if (!mv.moving) continue;

            Vec2 oldPos = player->pos();
            Vec2 newPos = oldPos;

            newPos.x += mv.dir.x * mv.speed * step;
            newPos.y += mv.dir.y * mv.speed * step;

            if (!is_walkable(oldPos, newPos)) {
                continue;
            }

            player->set_pos(newPos.x, newPos.y);

            if (aoiSystem_) {
                aoiSystem_->move_entity(pid, newPos.x, newPos.y);
            }
        }
    }
    void FieldWorker::tick_monsters(float step)
    {
        monsterWorld_.update(step, env_);
    }
    void FieldWorker::SpawnMonstersEvenGrid(int fieldId)
    {
           if (fieldId != 1000) return;

    constexpr int   kSpawnCount = 100;
    constexpr float kMinX = 0.f, kMaxX = 500.f;
    constexpr float kMinY = 0.f, kMaxY = 500.f;

    // 100 -> 10 x 10
    constexpr int cols = 10;
    constexpr int rows = 10;

    const float cellW = (kMaxX - kMinX) / cols; // 50
    const float cellH = (kMaxY - kMinY) / rows; // 50

    // 셀 중앙에 놓으면 가장 보기 좋고 충돌/겹침도 방지됨
    for (int i = 0; i < kSpawnCount; ++i)
    {
        const int r = i / cols;
        const int c = i % cols;

        float x = kMinX + (c + 0.5f) * cellW;
        float y = kMinY + (r + 0.5f) * cellH;

        x = clampf(x, kMinX, kMaxX);
        y = clampf(y, kMinY, kMaxY);

        const std::string& tpl = kMonsterTemplates[i % kMonsterTemplates.size()];
        auto monsterId = MakeDatabaseID(1);

        monsterWorld_.create_monster(monsterId, x, y, tpl);
        if (aoiSystem_) aoiSystem_->add_entity(monsterId, /*isPlayer=*/false, x, y);
    }
    }


    // 어딘가에 있는 헬퍼들

    std::shared_ptr<core::FieldWorker> GetFieldWorker(int fieldId)
    {
        return core::FieldManager::instance().get_field(fieldId);
    }

    bool SendToFieldWorker(int fieldId, core::NetMessage msg)
    {
        auto worker = core::FieldManager::instance().get_field(fieldId);
        if (!worker) {
            // 디버그용 로그 하나 넣어두면 좋음
            // LOG_ERROR("SendToFieldWorker: field {} not found", fieldId);
            return false;
        }

        worker->push(std::move(msg));
        return true;
    }


    // --------------------------------------------------------------------
    // (옵션) 클라 절대좌표 Move를 필드워커로 보내는 헬퍼
    //  - 서버 권한 이동으로 완전히 전환하면 안 써도 됨
    // --------------------------------------------------------------------
    void send_move_to_fieldworker(std::uint64_t playerId, int fieldId, float x, float y)
    {
        flatbuffers::FlatBufferBuilder fbb;

        auto pos = field::CreateVec2(fbb, x, y);

        // 플레이어가 보내는 Move이므로 entityType = Player, entityId = playerId
        auto cmd = field::CreateFieldCmd(
            fbb,
            field::FieldCmdType::FieldCmdType_Move,      // type
            field::EntityType::EntityType_Player,        // entityType
            playerId,                                    // entityId
            pos,                                         // pos
            0,                                            // dir (절대 좌표 전송용이니 비워둠)
            fbb.CreateString("")
        );
        fbb.Finish(cmd);

        NetMessage msg;
        msg.type = MessageType::Custom;
        msg.session = nullptr;
        msg.payload.assign(
            fbb.GetBufferPointer(),
            fbb.GetBufferPointer() + fbb.GetSize()
        );

        SendToFieldWorker(fieldId, std::move(msg));
    }

} // namespace core