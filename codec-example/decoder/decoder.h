#pragma once

#include <functional>
extern "C" {
#include <libavcodec/avcodec.h>
}
#include "common.hpp"

#include <memory>

namespace Codec {
    class BasicDecoder {
    public:
        using Ptr = std::shared_ptr<BasicDecoder>;

        explicit BasicDecoder(int chn)
            : chn_(chn){};
        virtual ~BasicDecoder(){};

        // 这里AVPacket->pts赋值为timestamp
        using DecodeCallback = std::function<void(uint64_t pktid, AVFrame *outframe)>;

        virtual int open(AVCodecID codecID, DecodeCallback callback) = 0;

        virtual int decode(uint64_t pktid, FGRecord::AVPacketSP fgpkt) = 0;

        virtual int flush(uint64_t ptkid) = 0;

        virtual int close() = 0;

        virtual AVBufferRef *getHwFramesCtx() = 0;

    protected:
        int chn_ = -1;
    };
} // namespace Codec