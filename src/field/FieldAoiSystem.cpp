// FieldAoiSystem.cpp
#include "FieldAoiSystem.h"

namespace core {

    FieldAoiSystem::FieldAoiSystem(int fieldId,
        float sectorSize,
        int   viewRadiusSectors,
        FieldAoiSendFunc sendFunc)
        : fieldId_(fieldId)
        , aoi_(sectorSize, viewRadiusSectors)
        , sendFunc_(std::move(sendFunc))
    {
        setup_aoi_callback();
    }

    void FieldAoiSystem::setup_aoi_callback()
    {
        // AoiWorld → 외부(FieldAoiSendFunc)로 이벤트 전달
        aoi_.set_send_callback(
            [this](std::uint64_t watcherId, const AoiEvent& ev)
            {
                if (sendFunc_) {
                    sendFunc_(watcherId, ev);
                }
            }
        );
    }



    void FieldAoiSystem::tick_update()
    {
        // 필요하면 나중에 전체 AOI 리프레시용으로 사용
    }


    // ---------------------------
    // 2) 서버 내부 로직 전용 API
    // ---------------------------

    void FieldAoiSystem::add_entity(std::uint64_t id, bool isPlayer, float x, float y)
    {
        AoiVec2 pos{ x, y };
        aoi_.add_entity(id, isPlayer, pos);

    }

    void FieldAoiSystem::move_entity(std::uint64_t id, float x, float y)
    {
        AoiVec2 pos{ x, y };
        aoi_.move_entity(id, pos);
    }

    void FieldAoiSystem::remove_entity(std::uint64_t id)
    {
        aoi_.remove_entity(id);
    }

} // namespace core
