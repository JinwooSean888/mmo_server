#pragma once
#include <uv.h>
#include <stdexcept>
#include <string>

namespace net {

    void uv_check(int rc, const char* where);

    struct Loop {
        uv_loop_t* loop{ nullptr };
        Loop();
        uv_loop_t* get() const;
    };

} // namespace net
