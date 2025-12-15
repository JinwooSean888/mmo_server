// net/tcp_server.cpp
#include "net/tcp_server.h"
#include "net/uv_utils.h"
#include "net/sessionManager.h"
#include "core/Dispatcher.h"
// 필요하면 Worker 헤더 추가
// #include "worker/worker.h"

namespace net {

    TcpServer::TcpServer(uv_loop_t* loop,
        const char* ip,
        int port,
        core::Dispatcher* disp,
        core::Worker* gameWorker)   // ★ 인자 추가
        : loop_(loop)
        , ip_(ip)
        , port_(port)
        , dispatcher_(disp)
        , gameWorker_(gameWorker)                    // ★ 멤버 초기화
    {
        uv_tcp_init(loop_, &server_);
        server_.data = this;
    }

    void TcpServer::start() {
        sockaddr_in addr{};
        net::uv_check(uv_ip4_addr(ip_, port_, &addr), "uv_ip4_addr");
        net::uv_check(
            uv_tcp_bind(&server_, reinterpret_cast<const sockaddr*>(&addr), 0),
            "uv_tcp_bind");
        net::uv_check(
            uv_listen(reinterpret_cast<uv_stream_t*>(&server_), 128, &TcpServer::on_new_conn),
            "uv_listen");
    }

    void TcpServer::on_new_conn(uv_stream_t* s, int status) {
        auto* self = reinterpret_cast<TcpServer*>(s->data);
        if (status < 0) {
            // 로그 정도
            return;
        }

        auto sess = std::make_shared<Session>(self->loop_, self->dispatcher_);

        // ★ 여기서 GameWorker 주입
        if (self->gameWorker_) {
            sess->set_game_worker(self->gameWorker_);
        }

        // close 콜백 등록
        sess->set_on_close([self](Session::Ptr closed) {
            SessionManager::instance().remove_session(closed->session_id());
            self->on_session_closed(closed);
            });

        if (uv_accept(s, sess->stream()) == 0) {
            sess->start();
			//플레이어 아이디 임시 할당
            sess->set_player_id(4);

            SessionManager::instance().add_session(sess);
            self->sessions_.push_back(sess);
        }
        else {
            // accept 실패 → sess 소멸
        }
    }

    void TcpServer::on_session_closed(const Session::Ptr& sess) {
        auto it = std::find(sessions_.begin(), sessions_.end(), sess);
        if (it != sessions_.end()) {
            sessions_.erase(it);
        }
    }

} // namespace net
