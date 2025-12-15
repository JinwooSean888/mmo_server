// AoiWorld.cpp
#include "AoiWorld.h"
#include <algorithm>
#include <cmath>

//--------------------------------------------
// ctor
//--------------------------------------------
AoiWorld::AoiWorld(float sectorSize, int viewRadiusSectors)
    : sectorSize_(sectorSize > 0.0f ? sectorSize : 1.0f)
    , viewRadius_(viewRadiusSectors > 0 ? viewRadiusSectors : 1)
{
}

//--------------------------------------------
// public: add/remove/move
//--------------------------------------------

void AoiWorld::add_entity(uint64_t id, bool isPlayer, const AoiVec2& pos)
{
    Entity e;
    e.id = id;
    e.isPlayer = isPlayer;
    e.pos = pos;
    e.sector = world_to_sector(pos);

    // 1. 엔티티 등록
    entities_[id] = e;
    enter_sector(id, e.sector);

    // 2. 플레이어인 경우 구독 섹터 계산(중요!)
    if (isPlayer) {
        rebuild_player_subscriptions(entities_[id]);
    }

    //// 3. 새 플레이어에게 기존 엔티티 스냅샷 전송
    //if (isPlayer && sendCb_) {
    //    for (auto& [otherId, other] : entities_) {
    //        if (otherId == id) continue;

    //        AoiEvent ev;
    //        ev.type = AoiEvent::Type::Snapshot;
    //        ev.subjectId = otherId;
    //        ev.position = other.pos;
    //        sendCb_(id, ev);
    //    }
    //}

    // 4. 기존 watcher들에게 Enter 알림
    if (sendCb_) {
        AoiEvent ev;
        ev.type = AoiEvent::Type::Enter;
        ev.subjectId = id;
        ev.position = pos;

        broadcast_to_sector_watchers(e.sector, ev, id);

    }
}



void AoiWorld::remove_entity(std::uint64_t id)
{
    auto it = entities_.find(id);
    if (it == entities_.end())
        return;

    Entity& e = it->second;

    // ★ 먼저 이 엔티티를 보고 있던 watcher들에게 Leave 알림
    if (sendCb_) {
        AoiEvent ev;
        ev.type = AoiEvent::Type::Leave;
        ev.subjectId = id;
        ev.position = e.pos;

        broadcast_to_sector_watchers(e.sector, ev, id);
    }

    // 섹터에서 제거
    leave_sector(id, e.sector);

    // 플레이어면 구독 섹터에서 watcher 제거
    if (e.isPlayer) {
        for (const auto& sc : e.subscribed) {
            auto sit = sectors_.find(sc);
            if (sit != sectors_.end()) {
                sit->second.watchers.erase(e.id);
            }
        }
        e.subscribed.clear();
    }

    entities_.erase(it);
}

void AoiWorld::move_entity(std::uint64_t id, const AoiVec2& newPos)
{
    auto it = entities_.find(id);
    if (it == entities_.end()) return;

    Entity& e = it->second;
    AoiSectorCoord oldSector = e.sector;

    e.pos = newPos;
    e.sector = world_to_sector(newPos);

    bool sectorChanged = !(e.sector == oldSector);

    // 2) 섹터 membership 이동
    if (sectorChanged) {
        leave_sector(id, oldSector);
        enter_sector(id, e.sector);
    }

    // 3) 플레이어라면 자기 구독 섹터 갱신
    if (e.isPlayer) {
        rebuild_player_subscriptions(e);
    }

    if (!sendCb_) return;

    // sectorChanged일 때만 old/new watcher 차집합으로 Leave/Enter
    if (sectorChanged) {
        const Sector* oldS = get_sector(oldSector);
        const Sector* newS = get_sector(e.sector);

        // oldWatchers - newWatchers => Leave
        if (oldS) {
            for (auto watcherId : oldS->watchers) {
                if (newS && newS->watchers.find(watcherId) != newS->watchers.end())
                    continue; // 여전히 볼 수 있음
                if (watcherId == id) continue;

                AoiEvent leaveEv;
                leaveEv.type = AoiEvent::Type::Leave;
                leaveEv.subjectId = id;
                leaveEv.position = e.pos;
                sendCb_(watcherId, leaveEv);
            }
        }

        // newWatchers - oldWatchers => Enter (또는 Snapshot)
        if (newS) {
            for (auto watcherId : newS->watchers) {
                if (oldS && oldS->watchers.find(watcherId) != oldS->watchers.end())
                    continue; // 예전부터 보고있음
                if (watcherId == id) continue;

                AoiEvent enterEv;
                enterEv.type = AoiEvent::Type::Enter; // Snapshot으로 해도 OK
                enterEv.subjectId = id;
                enterEv.position = e.pos;
                sendCb_(watcherId, enterEv);
            }
        }
    }

    // 4) Move는 "현재 섹터" watcher에게만
    {
        AoiEvent ev;
        ev.type = AoiEvent::Type::Move;
        ev.subjectId = id;
        ev.position = e.pos;

        broadcast_to_sector_watchers(e.sector, ev, id);
        if (e.isPlayer) {
            sendCb_(id, ev);
        }
    }
}



//--------------------------------------------
// public: player AOI만 갱신
//--------------------------------------------

void AoiWorld::update_player_aoi(std::uint64_t playerId)
{
    auto it = entities_.find(playerId);
    if (it == entities_.end())
        return;

    Entity& e = it->second;
    if (!e.isPlayer)
        return;

    rebuild_player_subscriptions(e);
}

//--------------------------------------------
// public: get_entity
//--------------------------------------------

const AoiWorld::Entity* AoiWorld::get_entity(std::uint64_t id) const
{
    auto it = entities_.find(id);
    return (it == entities_.end()) ? nullptr : &it->second;
}

AoiWorld::Entity* AoiWorld::get_entity(std::uint64_t id)
{
    auto it = entities_.find(id);
    return (it == entities_.end()) ? nullptr : &it->second;
}

//--------------------------------------------
// private: world_to_sector
//--------------------------------------------

AoiSectorCoord AoiWorld::world_to_sector(const AoiVec2& pos) const
{
    int sx = static_cast<int>(std::floor(pos.x / sectorSize_));
    int sy = static_cast<int>(std::floor(pos.y / sectorSize_));

    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;

    return { sx, sy };
}

//--------------------------------------------
// private: sector access
//--------------------------------------------

AoiWorld::Sector* AoiWorld::get_or_create_sector(const AoiSectorCoord& c)
{
    auto it = sectors_.find(c);
    if (it != sectors_.end())
        return &it->second;

    auto [insIt, _] = sectors_.emplace(c, Sector{});
    return &insIt->second;
}

const AoiWorld::Sector* AoiWorld::get_sector(const AoiSectorCoord& c) const
{
    auto it = sectors_.find(c);
    return (it == sectors_.end()) ? nullptr : &it->second;
}

//--------------------------------------------
// private: sector membership
//--------------------------------------------

void AoiWorld::enter_sector(std::uint64_t id, const AoiSectorCoord& c)
{
    Sector* s = get_or_create_sector(c);
    s->entities.insert(id);
}

void AoiWorld::leave_sector(std::uint64_t id, const AoiSectorCoord& c)
{
    auto it = sectors_.find(c);
    if (it == sectors_.end())
        return;
    it->second.entities.erase(id);
}

//--------------------------------------------
// private: view sectors (player)
//--------------------------------------------

void AoiWorld::collect_view_sectors(const Entity& e,
    std::vector<AoiSectorCoord>& out) const
{
    out.clear();
    for (int dy = -viewRadius_; dy <= viewRadius_; ++dy) {
        for (int dx = -viewRadius_; dx <= viewRadius_; ++dx) {
            AoiSectorCoord sc{ e.sector.x + dx, e.sector.y + dy };
            if (sc.x < 0 || sc.y < 0)
                continue;
            out.push_back(sc);
        }
    }
}

//--------------------------------------------
// private: rebuild_player_subscriptions
//--------------------------------------------

void AoiWorld::rebuild_player_subscriptions(Entity& e)
{
    // 1) 새 구독 섹터 목록 계산
    std::vector<AoiSectorCoord> viewSectors;
    collect_view_sectors(e, viewSectors);

    std::unordered_set<AoiSectorCoord, AoiSectorCoord::Hasher> newSet(
        viewSectors.begin(), viewSectors.end()
    );

    // 2) 제거/추가 diff
    std::vector<AoiSectorCoord> toRemove;
    std::vector<AoiSectorCoord> toAdd;

    for (const auto& oldSc : e.subscribed) {
        if (newSet.find(oldSc) == newSet.end())
            toRemove.push_back(oldSc);
    }
    for (const auto& newSc : newSet) {
        if (e.subscribed.find(newSc) == e.subscribed.end())
            toAdd.push_back(newSc);
    }

    // 3) watchers 갱신
    for (const auto& sc : toRemove) {
        auto it = sectors_.find(sc);
        if (it == sectors_.end()) continue;

        // watcher 제거 전에, 진짜로 안 보이게 되는 엔티티만 Leave 전송
        if (sendCb_) {
            for (auto otherId : it->second.entities) {
                if (otherId == e.id) continue;

                const Entity* other = get_entity(otherId);
                if (!other) continue;

                // ? other의 현재 섹터가 새 구독(newSet)에 있으면 계속 보임 => Leave 금지
                if (newSet.find(other->sector) != newSet.end())
                    continue;

                AoiEvent ev;
                ev.type = AoiEvent::Type::Leave;
                ev.subjectId = otherId;
                ev.position = other->pos;
                sendCb_(e.id, ev);
            }
        }

        it->second.watchers.erase(e.id);
    }


    for (const auto& sc : toAdd) {
        Sector* s = get_or_create_sector(sc);
        s->watchers.insert(e.id);

        // 새로 구독한 섹터의 모든 엔티티에 대해 Snapshot 이벤트 전송
        if (sendCb_) {
            for (auto otherId : s->entities) {
                if (otherId == e.id) continue;

                if (const Entity* other = get_entity(otherId)) {
                    AoiEvent ev;
                    ev.type = AoiEvent::Type::Snapshot;
                    ev.subjectId = other->id;
                    ev.position = other->pos;

                    sendCb_(e.id, ev);
                }
            }
        }
    }

    // 4) 플레이어의 구독 섹터 집합 최신화
    e.subscribed = std::move(newSet);
}

//--------------------------------------------
// private: broadcast_to_sector_watchers
//--------------------------------------------

void AoiWorld::broadcast_to_sector_watchers(const AoiSectorCoord& c,const AoiEvent& ev,std::int64_t excludeId)
{
    if (!sendCb_) return;

    auto it = sectors_.find(c);
    if (it == sectors_.end())
        return;

    for (auto watcherId : it->second.watchers) {
        if (excludeId != 0 && watcherId == excludeId)
            continue; // 본인 제외
        sendCb_(watcherId, ev);
    }
}
static void collect_watchers(const AoiWorld::Sector* s, std::vector<uint64_t>& out)
{
    out.clear();
    if (!s) return;
    out.reserve(s->watchers.size());
    for (auto w : s->watchers) out.push_back(w);
}