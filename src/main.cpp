
#include <csignal>
#include <thread>
#include <chrono>
#include <vector>
#include <uv.h>

#include "worker/worker.h"
#include "worker/FieldWorker.h"
#include "worker/workerManager.h"
#include "worker/codec.h"
#include "net/uv_utils.h"
#include "net/session.h"
#include "net/tcp_server.h"
#include "core/thread_pool.h"
#include "core/dispatcher.h"

#include "core/monitor/monitor.h"
#include "field/FieldManager.h"
#include "core/handlers/game_handler_registry.h"   // ★ 전체 게임 핸들러 등록

uv_loop_t* loop = nullptr;
struct ServerInitContext
{
    std::shared_ptr<core::Worker>      gameWorker;   // GameWorker
    std::shared_ptr<core::FieldWorker> mainField;    // 대표 필드 (지금은 1000번)
    std::vector<core::Worker*>         fieldWorkers; // 모니터링용
};
ServerInitContext SetupGameAndFields(core::Dispatcher& disp)
{
    ServerInitContext ctx;

  // ----- GameWorker 생성 -----
    core::CreateGameWorker();
    ctx.gameWorker = core::GetGameWorker();   // shared_ptr<core::Worker>

  // ----- FieldManager 를 통해 필드 여러 개 생성 -----
    auto& fm = core::FieldManager::instance();

  // 예: 1000 = 마을, 2000 = 사냥터1, 3000 = 던전 입구
    auto field1000 = fm.create_field(1000);
    auto field2000 = fm.create_field(2000);
    auto field3000 = fm.create_field(3000);

    if (!field1000) {
        std::cout << "[Fatal] FieldWorker(1000) 생성 실패\n";
    }

  // 모니터링용 필드 워커 목록
    if (field1000) ctx.fieldWorkers.push_back(field1000.get());
    if (field2000) ctx.fieldWorkers.push_back(field2000.get());
    if (field3000) ctx.fieldWorkers.push_back(field3000.get());

  // 대표 필드 하나 (라우팅용, 나중에 player->field_id 로 교체 예정)
    ctx.mainField = field1000;

  // ----- GameWorker on_message 설정 -----
    if (ctx.gameWorker) {
        auto fw = ctx.mainField; // 대표 필드 워커 캡처 (임시 라우팅용)

        ctx.gameWorker->set_on_message([&disp, fw](const core::NetMessage& msg) {
            if (msg.type != core::MessageType::NetEnvelope)
                return;
            if (!msg.session)
                return;

            const auto& buf = msg.payload;
            if (!proto::verify_envelope(buf.data(), buf.size()))
                return;

            auto* env = proto::get_envelope(buf.data());
            if (!env)
                return;

            auto msgType = env->pkt_type();  // game::MsgType

            switch (msgType) {
            case game::MsgType_Ping:
            case game::MsgType_Login:
            case game::MsgType_EnterField:
            case game::MsgType_SkillCmd:
              // 시스템/로그인/필드입장 등 GameWorker 레벨 처리
                disp.dispatch(*env, msg.session.get());
                break;

            default:
              // 나머지는 필드 쪽으로 라우팅 (현재는 1000번 필드로 고정)
                if (fw) {
                    fw->push(msg);
                }
                break;
            }
            });
    }

  // ----- 게임 패킷 핸들러 등록 -----
  //  (Ping, Login, EnterField 등은 전부 game_handlers 쪽에서 등록)
    handlers::RegisterAllGameHandlers(disp);

    return ctx;
}


int main() {
    // ----- 신호 처리 등록 -----
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    // ----- libuv 루프 -----
    net::Loop io_loop;
    loop = io_loop.get();   // 전역 loop 사용

    // ----- 디스패처 -----
    core::Dispatcher disp;

    // ----- 게임/필드/핸들러 초기화 -----
    auto init = SetupGameAndFields(disp);
    auto gameWorker = init.gameWorker;
    auto fieldWorker = init.mainField;
    auto& fieldWorkers = init.fieldWorkers;   // 모니터링용

    // ----- TcpServer 생성 및 시작 -----
    const char* listen_ip = "127.0.0.1";
    const int   listen_port = 9000;

    net::TcpServer server(
        loop,
        listen_ip,
        listen_port,
        &disp,
        gameWorker ? gameWorker.get() : nullptr  // ★ GameWorker 넘겨줌
    );
    server.start();

    // ----- TickWorkers (게임 틱 워커) -----
    const int   tick_threads = 3;
    const int   tick_ms = 50;           // 20Hz
    const float tick_dt = tick_ms / 1000.0f;

    core::TickWorkers game_workers(tick_threads, tick_ms);

    game_workers.on_tick([&](int idx) {
        auto& fm = core::FieldManager::instance();
        fm.for_each_field([&](const std::shared_ptr<core::FieldWorker>& fw) {
            if (!fw) return;

            // FieldWorker에 field_id() 같은 게 있다고 가정
            const uint32_t fid = fw->field_id();

            // 이 쓰레드 담당 필드만 처리
            if ((fid % tick_threads) != (uint32_t)idx)
                return;

            fw->update_world(tick_dt);
            });
        });

    // ----- 서버/워커 정보 출력 -----
    PrintServerInfo(
        listen_ip,
        listen_port,
        tick_threads,
        tick_ms,
        gameWorker ? gameWorker.get() : nullptr,
        fieldWorkers
    );

    // ----- 큐 모니터 스레드 시작 -----
    std::thread monitor_thread = StartWorkerQueueMonitor(
        gameWorker ? gameWorker.get() : nullptr,
        fieldWorkers
    );

    // ----- 메인 루프 -----
    while (g_running.load()) {
        uv_run(loop, UV_RUN_NOWAIT);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "\n[Server] Shutdown requested. Stopping workers...\n";

    if (monitor_thread.joinable())
        monitor_thread.join();

    // ----- 종료 정리 -----
    core::FieldManager::instance().stop_all();
    core::WorkerManager::instance().stop_all();

    std::cout << "[Server] All workers stopped. Bye.\n";

    return 0;
}
