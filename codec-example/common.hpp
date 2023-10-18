#pragma once

#include <iostream>
#include <memory>

#define LOG_CHN(a, b) std::cout << b
#define LOG(a)        std::cout

namespace FGRecord {
    class AVPacket {
    public:
        uint64_t                 timestamp = 0;
        uint32_t                 fps       = 0;
        uint32_t                 size      = 0;
        std::shared_ptr<uint8_t> data      = nullptr;

    public:
        AVPacket(){};
        ~AVPacket() {
            if (data != nullptr) {
                data.reset();
                data = nullptr;
            }
        }
    };
    typedef std::shared_ptr<AVPacket> AVPacketSP;
} // namespace FGRecord