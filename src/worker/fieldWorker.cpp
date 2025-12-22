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
#include "field/monster/MonsterWorld.h"
#include "field/monster/MonsterEnvironment.h"
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
        , monsterWorld_()
        , env_(monsterWorld_)
    {
        init_monster_env();

        // 1) AOI 시스템 생성
        aoiSystem_ = std::make_shared<FieldAoiSystem>(fieldId_, 15.0f, 2);

        // 2) 콜백 등록
        aoiSystem_->set_send_func(
            [this](std::uint64_t watcherId, const AoiEvent& ev)
            {
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

                bool isMonster = is_monster_id(ev.subjectId);
                field::EntityType et = isMonster
                    ? field::EntityType::EntityType_Monster
                    : field::EntityType::EntityType_Player;

                std::string prefabName = get_prefab_name(ev.subjectId, isMonster);
                if (prefabName.empty())
                    prefabName = "Default";

                auto prefabStr = fbb.CreateString(prefabName);

                auto cmd = field::CreateFieldCmd(
                    fbb,
                    cmdType,
                    et,
                    ev.subjectId,
                    pos,
                    0,
                    prefabStr
                );

                auto envOffset = field::CreateEnvelope(
                    fbb,
                    field::Packet::Packet_FieldCmd,
                    cmd.Union()
                );

                fbb.Finish(envOffset);

                sess->send_payload(
                    fbb.GetBufferPointer(),
                    static_cast<std::uint32_t>(fbb.GetSize())
                );
            }
        );

        set_on_message(
            [this](const NetMessage& msg)
            {
                handle_message(msg);
            }
        );

        if (fieldId == 1000) {
            SpawnMonstersEvenGrid(1000);
        }

        // 마지막에 AOI 활성화
        aoiSystem_->set_initialized(true);
    }




    FieldWorker::~FieldWorker() = default;

    void FieldWorker::handle_message(const NetMessage& msg)
    {
        if (!aoiSystem_) return;

        if (msg.type == MessageType::Custom)
        {
            const uint8_t* buf = msg.payload.data();

            auto env = field::GetEnvelope(buf);
            if (!env) {
                std::cout << "Invalid field envelope";
                return;
            }

            if (env->pkt_type() != field::Packet::Packet_FieldCmd)
                return;

            auto* cmd = env->pkt_as_FieldCmd();
            if (!cmd) return;

            if (cmd->type() == field::FieldCmdType::FieldCmdType_Move)
            {				
                on_client_move_input(*cmd, msg.session);
            }
            // Enter/Leave 를 여기서도 쓰면 추가 분기

            return;
        }
        else if (msg.type == MessageType::SkillCmd)
        {
            handle_skill(msg);
            return;
        }
    }



    void FieldWorker::on_client_move_input(const field::FieldCmd& cmd, net::Session::Ptr session)
    {
        if (!session) return;

        if (session->player_id() != cmd.entityId())
            return;

        if (cmd.type() != field::FieldCmdType::FieldCmdType_Move)
            return;

        auto it = players_.find(cmd.entityId());
        if (it == players_.end())
            return;

        auto* dir = cmd.dir();
        if (!dir)
            return;

        float dx = dir->x();
        float dy = dir->y();
        float len2 = dx * dx + dy * dy;

        auto& mv = it->second->move_state();

        // 입력 시간은 항상 worldTime_ 기준
        mv.lastInputTime = worldTime_;

        // 정지 입력인 경우: 바로 Idle 브로드캐스트
        if (len2 < 1e-4f) {
            if (mv.moving) {
                mv.moving = false;
                mv.dir = { 0.f, 0.f };
                mv.speed = 0.f;

                env_.broadcastPlayerState(cmd.entityId(), monster_ecs::PlayerState::Idle);
            }
            return;
        }

        // 방향 입력 있는 경우: 새로 움직이기 시작하는 순간에만 Move 브로드캐스트
        float invLen = 1.0f / std::sqrt(len2);
        dx *= invLen;
        dy *= invLen;

        bool wasMoving = mv.moving;
        mv.moving = true;
        mv.dir = { dx, dy };
        mv.speed = 4.5f;

        if (!wasMoving) {
            env_.broadcastPlayerState(cmd.entityId(), monster_ecs::PlayerState::Move);
        }
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

    void FieldWorker::send_combat_event(field::EntityType attackerType, uint64_t attackerId, field::EntityType targetType, uint64_t targetId,
        int damage, int remainHp)
    {
        auto sess = net::SessionManager::instance().find_by_player_id(targetId);
        if (!sess) return;

        flatbuffers::FlatBufferBuilder fbb;

        auto evOffset = field::CreateCombatEvent(
            fbb,
            attackerType,
            attackerId,
            targetType,
            targetId,
            damage,
            remainHp
        );

        auto envOffset = field::CreateEnvelope(
            fbb,
            field::Packet::Packet_CombatEvent,
            evOffset.Union()
        );

        fbb.Finish(envOffset);

        sess->send_payload(
            fbb.GetBufferPointer(),
            static_cast<std::uint32_t>(fbb.GetSize())
        );
    }
    void FieldWorker::send_stat_event(std::uint64_t watcherId, std::uint64_t subjectId, bool isMonster, int hp, int maxHp, int sp, int maxSp)
    {
        auto sess = net::SessionManager::instance().find_by_player_id(watcherId);
        if (!sess) return;

        flatbuffers::FlatBufferBuilder fbb;

        field::EntityType et = isMonster
            ? field::EntityType::EntityType_Monster
            : field::EntityType::EntityType_Player;

        auto statOffset = field::CreateStatEvent(
            fbb,
            et,
            subjectId,
            hp,
            maxHp,
            sp,
            maxSp
        );

        auto envOffset = field::CreateEnvelope(
            fbb,
            field::Packet::Packet_StatEvent,
            statOffset.Union()
        );

        fbb.Finish(envOffset);

        sess->send_payload(
            fbb.GetBufferPointer(),
            static_cast<std::uint32_t>(fbb.GetSize())
        );
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

        auto skillType = skill->skill();
        uint64_t targetId = skill->targetId();

        std::cout << "[SKILL] Player " << pid
            << " attacked target=" << targetId
            << " skill=" << static_cast<int>(skill->skill())
            << std::endl;

		env_.broadcastPlayerState(pid, monster_ecs::PlayerState::Attack);
        // ?? 모든 사망/리스폰/AOI 처리는 MonsterWorld 내부에서
        bool dead = monsterWorld_.player_attack_monster(pid, targetId, skillType, env_);

        if (dead)
        {
			env_.broadcastAiState(targetId, monster_ecs::CAI::State::Dead);
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
    void FieldWorker::send_field_enter(std::uint64_t watcherId, std::uint64_t subjectId, bool isMonster, const Vec2& pos)
    {
        auto sess = net::SessionManager::instance().find_by_player_id(watcherId);
        if (!sess) return;

        flatbuffers::FlatBufferBuilder fbb;

        auto posOffset = field::CreateVec2(fbb, pos.x, pos.y);

        field::EntityType et = isMonster
            ? field::EntityType::EntityType_Monster
            : field::EntityType::EntityType_Player;

        std::string prefabName = get_prefab_name(subjectId, isMonster);
        auto prefabStr = fbb.CreateString(prefabName);

        auto cmd = field::CreateFieldCmd(
            fbb,
            field::FieldCmdType::FieldCmdType_Enter,
            et,
            subjectId,
            posOffset,
            0,
            prefabStr
        );

        auto envOffset = field::CreateEnvelope(
            fbb,
            field::Packet::Packet_FieldCmd,
            cmd.Union()
        );

        fbb.Finish(envOffset);

        sess->send_payload(
            fbb.GetBufferPointer(),
            static_cast<std::uint32_t>(fbb.GetSize())
        );
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

                send_combat_event(
                    field::EntityType::EntityType_Monster, monsterId,
                    field::EntityType::EntityType_Player, playerId,
                    0, st.hp
                );
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

        env_.broadcastAiState = [this](uint64_t monsterId, monster_ecs::CAI::State newState) {
            broadcast_monster_ai_state(monsterId, newState);
            };

        env_.broadcastPlayerState = [this](uint64_t playerId, monster_ecs::PlayerState st) {
            broadcast_player_ai_state(playerId, st);
            };

        env_.broadcastMonsterStat =
            [this](uint64_t monsterId, int hp, int maxHp, int sp, int maxSp)
            {
                broadcast_monster_stat(monsterId, hp, maxHp, sp, maxSp);
            };
        
        env_.broadcastPlayerStat =
            [this](uint64_t playerId, int hp, int maxHp, int sp, int maxSp)
            {
                broadcast_player_stat(playerId, hp, maxHp, sp, maxSp);
            };

        env_.getPlayerStats = [this](uint64_t pid,
            int& hp, int& maxHp,
            int& sp, int& maxSp)
            {
                auto p = PlayerManager::instance().get_by_id(pid);
                if (!p) return false;

                hp = p->hp();
                maxHp = p->max_hp();
                sp = p->sp();
                maxSp = p->max_sp();
                return true;
            };

        env_.setPlayerStats = [this](uint64_t pid, int hp, int sp)
            {
                auto p = PlayerManager::instance().get_by_id(pid);
                if (!p) return;

                p->set_hp(hp);
                p->set_sp(sp);
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
                env_.broadcastPlayerState(pid, monster_ecs::PlayerState::Idle);
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

        constexpr int kSpawnCount = 300;
        constexpr float kMinX = 0.f, kMaxX = 500.f;
        constexpr float kMinY = 0.f, kMaxY = 500.f;

        constexpr int cols = 10;
        constexpr int rows = 10;

        const float cellW = (kMaxX - kMinX) / cols;
        const float cellH = (kMaxY - kMinY) / rows;

        for (int i = 0; i < kSpawnCount; ++i)
        {
            const int r = i / cols;
            const int c = i % cols;

            float x = kMinX + (c + 0.5f) * cellW;
            float y = kMinY + (r + 0.5f) * cellH;

            x = clampf(x, kMinX, kMaxX);
            y = clampf(y, kMinY, kMaxY);

            const MonsterTemplate& tpl = kMonsterTemplates[i % kMonsterTemplates.size()];

            const int typeIndex = i % kMonsterTemplates.size();

            int monsterType = 0;
            if (typeIndex < 3)
                monsterType = 1;     // Bow
            else if (typeIndex < 6)
                monsterType = 2;     // DoubleSword
            else
                monsterType = 3;     // MagicWand

            uint64_t monsterId = MakeDatabaseID(1);

            monsterWorld_.create_monster(
                monsterId,
                x, y,
                tpl.name,
                monsterType,
                tpl.maxHp,
                tpl.hp,
                tpl.maxSp,
                tpl.sp,
                tpl.atk,
                tpl.def
            );

            if (aoiSystem_)
                aoiSystem_->add_entity(monsterId, false, x, y);
        }
    }


    void FieldWorker::broadcast_ai_state(uint64_t entityId, field::EntityType et, field::AiStateType fbState)
    {
        aoiSystem_->for_each_watcher(entityId, [&](uint64_t watcherId) {
            auto sess = net::SessionManager::instance().find_by_player_id(watcherId);
            if (!sess) return;

            flatbuffers::FlatBufferBuilder fbb;

            auto evOffset = field::CreateAiStateEvent(
                fbb,
                et,
                entityId,
                fbState
            );

            auto envOffset = field::CreateEnvelope(
                fbb,
                field::Packet::Packet_AiStateEvent,
                evOffset.Union()
            );

            fbb.Finish(envOffset);

            sess->send_payload(
                fbb.GetBufferPointer(),
                static_cast<std::uint32_t>(fbb.GetSize())
            );
            });
    }

    void FieldWorker::broadcast_monster_ai_state(uint64_t monsterId, monster_ecs::CAI::State newState)
    {

        broadcast_ai_state(
            monsterId,
            field::EntityType::EntityType_Monster,
            to_fb_state(newState)
        );
    }
    void FieldWorker::broadcast_player_ai_state(uint64_t playerId, monster_ecs::PlayerState st)
    {
        std::cout << "[BCAST_PLAYER_STATE] pid=" << playerId
            << " st=" << (int)st
            << " fbState=" << (int)st << std::endl;

        broadcast_ai_state(
            playerId,
            field::EntityType::EntityType_Player,
            to_fb_state(st)
        );
    }
    void FieldWorker::broadcast_stat_event(uint64_t entityId, field::EntityType et, int hp, int maxHp, int sp, int maxSp)
    {
        aoiSystem_->for_each_watcher(entityId, [&](uint64_t watcherId)
            {
                auto sess = net::SessionManager::instance().find_by_player_id(watcherId);
                if (!sess) return;

                flatbuffers::FlatBufferBuilder fbb;

                auto evOffset = field::CreateStatEvent(
                    fbb,
                    et,
                    entityId,
                    hp,
                    maxHp,
                    sp,
                    maxSp
                );

                auto envOffset = field::CreateEnvelope(
                    fbb,
                    field::Packet::Packet_StatEvent,
                    evOffset.Union()
                );

                fbb.Finish(envOffset);

                sess->send_payload(
                    fbb.GetBufferPointer(),
                    static_cast<std::uint32_t>(fbb.GetSize())
                );
            });
    }
    void FieldWorker::broadcast_monster_stat(uint64_t monsterId,int hp, int maxHp,int sp, int maxSp)
    {
        broadcast_stat_event(
            monsterId,
            field::EntityType::EntityType_Monster,
            hp, maxHp,
            sp, maxSp
        );
    }
    void FieldWorker::broadcast_player_stat(uint64_t playerId,int hp, int maxHp,int sp, int maxSp)
    {
        broadcast_stat_event(
            playerId,
            field::EntityType::EntityType_Player,
            hp, maxHp,
            sp, maxSp
        );
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

            return false;
        }

        worker->push(std::move(msg));
        return true;
    }


    void send_move_to_fieldworker(std::uint64_t playerId, int fieldId, float x, float y)
    {
        flatbuffers::FlatBufferBuilder fbb;

        auto pos = field::CreateVec2(fbb, x, y);

        auto cmd = field::CreateFieldCmd(
            fbb,
            field::FieldCmdType::FieldCmdType_Move,   // type
            field::EntityType::EntityType_Player,     // entityType
            playerId,                                 // entityId
            pos,                                      // pos
            0,                                        // dir (절대 좌표 전송이라 0)
            fbb.CreateString("")                      // prefab (안씀)
        );

        // FieldCmd를 Envelope로 포장
        auto envOffset = field::CreateEnvelope(
            fbb,
            field::Packet::Packet_FieldCmd,           // union 타입
            cmd.Union()
        );

        fbb.Finish(envOffset);

        NetMessage msg;
        msg.type = MessageType::Custom;
        msg.session = nullptr;                        // 내부용이니까 세션 없음
        msg.payload.assign(
            fbb.GetBufferPointer(),
            fbb.GetBufferPointer() + fbb.GetSize()
        );

        SendToFieldWorker(fieldId, std::move(msg));
    }



} // namespace core