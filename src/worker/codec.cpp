
#include "codec.h"

#include <cstring>
#include <flatbuffers/flatbuffers.h>
#include "proto/generated/field_generated.h"   // game::VerifyEnvelopeBuffer
#include "proto/generated/game_generated.h"   // game::VerifyEnvelopeBuffer

namespace proto {

    std::uint32_t Frame::read_len(const std::uint8_t* data) {
        std::uint32_t len;
        std::memcpy(&len, data, 4);
        return len;   // little-endian 환경(Windows/x64 등) 기준
    }

    void Frame::write(std::vector<std::uint8_t>& out,
        const std::uint8_t* payload,
        std::uint32_t len) {
        const std::size_t old = out.size();
        out.resize(old + kHeader + len);

        // length prefix
        std::memcpy(out.data() + old, &len, 4);
        // payload
        std::memcpy(out.data() + old + kHeader, payload, len);
    }

    bool verify_envelope(const std::uint8_t* buf, std::size_t len) {
        flatbuffers::Verifier verifier(buf, len);
        // 이 함수도 game_generated.h 안에 inline 으로 구현되어 있음
        return game::VerifyEnvelopeBuffer(verifier);
    }



} // namespace proto
