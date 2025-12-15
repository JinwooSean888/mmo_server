#include "net/uv_utils.h"

namespace net {

    void uv_check(int rc, const char* where) {
        if (rc < 0) {
            throw std::runtime_error(std::string(where) + ": " + uv_strerror(rc));
        }
    }

    Loop::Loop() {
        loop = uv_default_loop();
    }

    uv_loop_t* Loop::get() const {
        return loop;
    }

} // namespace net
