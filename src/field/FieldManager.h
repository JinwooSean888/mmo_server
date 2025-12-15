#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>

#include "worker/FieldWorker.h"   // FieldWorker 가 core::Worker 상속한다고 가정

namespace core {

    class FieldManager {
    public:
        static FieldManager& instance()
        {
            static FieldManager inst;
            return inst;
        }

        std::shared_ptr<FieldWorker> create_field(int fieldId);
        std::shared_ptr<FieldWorker> get_field(int fieldId);
        void stop_all();

        // -----------------------------------------------------------------
        // 모든 필드 워커에 대해 fn(fieldWorker) 호출
        //  - TickWorkers에서 필드별 update_world(dt) 호출 등에 사용
        // -----------------------------------------------------------------
        template <typename Fn>
        void for_each_field(Fn&& fn)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [id, w] : fields_) {
                if (w) {
                    fn(w);
                }
            }
        }

    private:
        FieldManager() = default;
        ~FieldManager() = default;

        FieldManager(const FieldManager&) = delete;
        FieldManager& operator=(const FieldManager&) = delete;

    private:
        std::mutex mutex_;
        std::unordered_map<int, std::shared_ptr<FieldWorker>> fields_;
    };

} // namespace core