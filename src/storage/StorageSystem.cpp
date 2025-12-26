#include "storage/StorageSystem.h"
#include "storage/DirtyHub.h"
#include "storage/DBworker/DbJob.h"
#include "storage/DBWorker.h"
#include <uv.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <iostream>

namespace storage {

    // ---- StorageSystem::Impl ----
    struct StorageSystem::Impl {
        uv_loop_s* loop{ nullptr };
        uv_timer_t timer{};
        std::atomic<bool> started{ false };

        config::ServerConfig cfg;
        DirtyHub dirty;
        DBWorker db;
        
        // flush interval (cfg에서 기본값 로드)
        std::uint64_t interval_ms{ 2000 };
        Impl(uv_loop_s* l, const config::ServerConfig& c)
            : loop(l)
            , cfg(c)
            , db([this](const DbJob& job) { this->handle_db_job(job); }) 
        {
            timer.data = this;
        }

        static void on_timer(uv_timer_t* t) {
            auto* self = static_cast<Impl*>(t->data);
            if (!self || !self->started.load()) return;

            // 1) dirty 스냅샷
            auto uids = self->dirty.steal_all();
            if (uids.empty()) return;

            // 2) DB 스레드로 push
            DbJob job;
            job.uids = std::move(uids);
            self->db.push(std::move(job));
        }

        bool start(std::uint64_t ms) {
            if (started.exchange(true)) return true;
            interval_ms = ms;

            db.start(); 

            int rc = uv_timer_init(loop, &timer);
            if (rc != 0) return false;

            rc = uv_timer_start(&timer, &Impl::on_timer, 100, interval_ms);
            return rc == 0;
        }

        void stop() {
            if (!started.exchange(false)) return;

            uv_timer_stop(&timer);
            uv_close(reinterpret_cast<uv_handle_t*>(&timer), nullptr);

            db.stop();
            db.join();

            std::cout << "[Storage] stopped.\n";
        }
        bool start() {
            return start(cfg.storage.flush_interval_ms);
        }
        void handle_db_job(const DbJob& job) {
            // cfg 사용 가능
            // TODO: 여기서 Redis pipeline + MySQL upsert
            std::cout << "[DBWorker] flush batch size=" << job.uids.size()
                << " flush_ms=" << cfg.storage.flush_interval_ms << "\n";
        }
    };

    // ---- StorageSystem public ----
    StorageSystem::StorageSystem() = default;
    StorageSystem::~StorageSystem() = default;

    StorageSystem::StorageSystem(StorageSystem&&) noexcept = default;
    StorageSystem& StorageSystem::operator=(StorageSystem&&) noexcept = default;
    StorageSystem StorageSystem::Create(uv_loop_s* loop, const config::ServerConfig& cfg) {
        StorageSystem ss;
        ss.impl_ = std::make_unique<Impl>(loop, cfg); // cfg 전달
        return ss;
    }

    void StorageSystem::start() {
        if (!impl_) return;
        if (!impl_->start()) std::cout << "[Storage] start failed\n";
    }

    void StorageSystem::stop() {
        if (!impl_) return;
        impl_->stop();
    }

    DirtyHub& StorageSystem::dirty() {
        return impl_->dirty;
    }

} // namespace storage
