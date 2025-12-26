#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <vector>
#include <cstdint>
#include <functional>
#include "storage/DBworker/DbJob.h"
namespace storage {


    class DBWorker {
    public:
        using Handler = std::function<void(const DbJob&)>;

        explicit DBWorker(Handler handler);

        void start();
        void stop();   // graceful stop signal
        void join();

        void push(DbJob job);

    private:
        void run();

    private:
        std::atomic<bool> running_{ false };
        std::thread th_;

        std::mutex mu_;
        std::condition_variable cv_;
        std::queue<DbJob> q_;

        Handler handler_;
    };

} // namespace storage
