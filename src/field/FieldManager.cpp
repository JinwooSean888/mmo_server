#include "field/FieldManager.h"

namespace core {

    std::shared_ptr<FieldWorker> FieldManager::create_field(int fieldId)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = fields_.find(fieldId);
        if (it != fields_.end()) {
            return it->second;
        }

        auto worker = std::make_shared<FieldWorker>(fieldId);
        worker->start();
        fields_[fieldId] = worker;
        return worker;
    }

    std::shared_ptr<FieldWorker> FieldManager::get_field(int fieldId)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = fields_.find(fieldId);
        if (it != fields_.end()) {
            return it->second;
        }
        return nullptr;
    }

    void FieldManager::stop_all()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, w] : fields_) {
            if (w) {
                w->stop();
            }
        }
        fields_.clear();
    }

} // namespace core