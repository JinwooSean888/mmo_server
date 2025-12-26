#pragma once
#include <cstdint>
#include <vector>

namespace storage {

    // DBWorker로 넘길 flush 단위 payload
    // - 기본은 uid 배치
    // - 추후: 이유/타입/타임스탬프/플래그 등 확장 가능
    struct DbJob
    {
        std::vector<std::uint64_t> uids;

        // (옵션) 디버깅/튜닝용 메타
        std::uint64_t enqueued_ms{ 0 };   // job 생성 시각(ms)
        std::uint32_t batch_id{ 0 };      // 배치 식별자 (선택)

        DbJob() = default;
        explicit DbJob(std::vector<std::uint64_t>&& ids)
            : uids(std::move(ids)) {
        }

        bool empty() const noexcept { return uids.empty(); }
        std::size_t size() const noexcept { return uids.size(); }

        void clear() {
            uids.clear();
            enqueued_ms = 0;
            batch_id = 0;
        }
    };

} // namespace storage
