#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include "core/core_types.h"

namespace net {
    class Session; // forward declaration (mmorpg_skel 의 net::Session 과 연결)
}

namespace core {

    // 워커가 처리할 메시지 타입 (필요하면 더 추가해도 됨)
    enum class MessageType : uint8_t {
        NetEnvelope = 0,   // 네트워크 패킷(FlatBuffers Envelope)
        Custom = 1,   // 기타 용도
        EnterField = 2,   // 필드 입장
        LeaveField = 3,   // 필드 퇴장
        MoveField = 4,   // 필드 내 이동
		SkillCmd = 5,   // 스킬 커맨드
    };

    // 네트워크 메시지: 어떤 세션에서 온 어떤 payload인가
    struct NetMessage {
        MessageType                      type{ MessageType::NetEnvelope };
        std::shared_ptr<net::Session>    session;   // 보낸 세션
        std::vector<uint8_t>             payload;   // FlatBuffers raw bytes
    };

    // 단일 워커스레드
    class Worker {
    public:
        using Ptr = std::shared_ptr<Worker>;
        using Callback = std::function<void(const NetMessage&)>;

        explicit Worker(std::string name);
        virtual ~Worker();

        // 워커 스레드 시작/정지
        void start();
        void stop();

        // 모니터링
        INT32 GetMessageCount();

        // 메시지 enqueue
        void push(NetMessage msg);

        // 메시지 처리 콜백 (워커 스레드 안에서 호출됨)
        void set_on_message(Callback cb);

        const std::string& name() const { return name_; }

        // ?? 모니터링용: OS 쓰레드 핸들 반환
        std::thread::native_handle_type native_handle() {
            return thread_.native_handle();
        }

    private:
        void loop(); // 내부 스레드 루프

        std::string              name_;
        std::atomic<bool>        running_{ false };
        std::thread              thread_;

        std::mutex               mutex_;
        std::condition_variable  cv_;
        std::queue<NetMessage>   queue_;
        Callback                 on_message_;
    };

    // 편의 함수: "GameWorker"라는 이름으로 하나 관리하고 싶을 때
    inline constexpr char GAME_WORKER_NAME[] = "GameWorker";

    Worker::Ptr  GetGameWorker();          // 없으면 nullptr
    bool         CreateGameWorker();       // 없으면 생성
    bool         SendToGameWorker(NetMessage msg);

} // namespace core
