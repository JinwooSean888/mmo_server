#pragma once
#include <cstdint>
#include <memory>
#include "config/server_config.h"
struct uv_loop_s;

namespace storage {

    class DirtyHub;

    // 외부에 보이는 API만
    class StorageSystem {
    public:
        StorageSystem();
        ~StorageSystem();

        StorageSystem(StorageSystem&&) noexcept;
        StorageSystem& operator=(StorageSystem&&) noexcept;

        StorageSystem(const StorageSystem&) = delete;
        StorageSystem& operator=(const StorageSystem&) = delete;

        static StorageSystem Create(uv_loop_s* loop, const config::ServerConfig& cfg);

        void start();
        void stop(); // flush stop -> db stop(join)

        // FieldWorker에서 더티 찍을 때 사용 (가장 중요한 노출 포인트)
        DirtyHub& dirty();

    private:
        struct Impl;                 // PIMPL
        std::unique_ptr<Impl> impl_; // 헤더에서 incomplete type 문제 원천 차단
    };

} // namespace storage
