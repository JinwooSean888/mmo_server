#pragma once
#include <unordered_set>
#include <vector>
#include <mutex>
#include <cstdint>

namespace storage {

    class DirtyHub {
    public:
        static constexpr std::size_t kShards = 64; // 2의 제곱 추천

        void mark_dirty(std::uint64_t uid) {
            auto& shard = shards_[uid & (kShards - 1)];
            std::lock_guard<std::mutex> lk(shard.mu);
            shard.set.insert(uid);
        }

        // 모든 shard를 합쳐서 가져감
        std::vector<std::uint64_t> steal_all() {
            std::vector<std::uint64_t> out;
            // 1) 대충 reserve (정확도 필요 없으면 생략 가능)
            // 2) shard별로 잠깐씩만 lock
            for (auto& s : shards_) {
                std::lock_guard<std::mutex> lk(s.mu);
                out.insert(out.end(), s.set.begin(), s.set.end());
                s.set.clear();
            }
            return out;
        }

    private:
        struct Shard {
            std::mutex mu;
            std::unordered_set<std::uint64_t> set;
        };
        Shard shards_[kShards];
    };

} // namespace storage
