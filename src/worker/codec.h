#pragma once

#include <cstdint>
#include <vector>

// FlatBuffers에서 자동 생성한 헤더
#include "generated/game_generated.h"   // 경로는 프로젝트에 맞게: src/core/proto/Generated/... 라면 그에 맞춰 수정
#include "generated/field_generated.h"

namespace proto {

    // 4-byte length prefix framing: | uint32 length | payload |
    struct Frame {
        static constexpr std::size_t kHeader = 4;

        // length prefix 읽기 (little-endian 가정)
        static std::uint32_t read_len(const std::uint8_t* data);

        // out 벡터 뒤에 [len][payload] 형식으로 append
        static void write(std::vector<std::uint8_t>& out,
            const std::uint8_t* payload,
            std::uint32_t len);
    };

    // FlatBuffers Envelope 검증
    bool verify_envelope(const std::uint8_t* buf, std::size_t len);

    // FlatBuffers Envelope 객체 포인터 얻기
    inline const game::Envelope* get_envelope(const std::uint8_t* buf) {
        // game_generated.h 안에 있는 inline GetEnvelope(...) 사용
        // 보통 시그니처가 const void* 이라서 캐스팅
        return game::GetEnvelope(reinterpret_cast<const void*>(buf));
    }


    inline bool verify_field_cmd(const uint8_t* buf, uint32_t len) {
        flatbuffers::Verifier v(buf, len);
        return field::VerifyFieldCmdBuffer(v);
    }

} // namespace proto
