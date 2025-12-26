#include "storage/DBWorker.h"
namespace storage {

    DBWorker::DBWorker(Handler handler)
        : handler_(std::move(handler)) {
    }

    void DBWorker::start() {
        running_.store(true);
        th_ = std::thread([this] { run(); });
    }

    void DBWorker::stop() {
        running_.store(false);
        cv_.notify_all();
    }

    void DBWorker::join() {
        if (th_.joinable()) th_.join();
    }

    void DBWorker::push(DbJob job) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            q_.push(std::move(job));
        }
        cv_.notify_one();
    }

    void DBWorker::run() {
        while (true) {
            DbJob job;
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(lk, [&] { return !q_.empty() || !running_.load(); });

                if (!running_.load() && q_.empty())
                    break;

                job = std::move(q_.front());
                q_.pop();
            }

            if (handler_) handler_(job);
        }
    }

} // namespace storage
