// FieldAoiSystem.h
#pragma once

#include <functional>
#include <memory>
#include <cstdint>

#include "worker/worker.h"     // NetMessage, MessageType
#include "AoiWorld.h"   // AoiWorld, AoiEvent
#include "generated/field_generated.h"  // ★ field::FieldCmd, field::FieldCmdType

namespace core {

    using FieldAoiSendFunc = std::function<void(std::uint64_t watcherId,
        const AoiEvent& ev)>;

    class FieldAoiSystem
    {
    public:
        FieldAoiSystem(int fieldId,
            float sectorSize,
            int   viewRadiusSectors,
            FieldAoiSendFunc sendFunc);

        void tick_update();

        // 2) 서버 내부에서 직접 쓰는 AOI API
        void add_entity(std::uint64_t id, bool isPlayer, float x, float y);
        void move_entity(std::uint64_t id, float x, float y);
        void remove_entity(std::uint64_t id);
        using Callback = std::function<void(std::uint64_t watcherId, const AoiEvent& ev)>;
                
        void for_each_watcher(uint64_t subjectId, std::function<void(uint64_t watcherId)> fn);
    private:
        int fieldId_;
        AoiWorld      aoi_;
        FieldAoiSendFunc sendFunc_;
        Callback callback_;
        std::unordered_map<uint64_t,std::vector<uint64_t>> watchers_;

        void setup_aoi_callback();
     
    };

} // namespace core
