#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "decoder.h"

#include <functional>
#include <memory>
#include <string>

/**
 * @brief 解码器-软件
 *
 */

namespace Codec {
    class SwDecoder : public BasicDecoder {
    public:
        explicit SwDecoder(int chn)
            : BasicDecoder(chn){};
        virtual ~SwDecoder(){};

        // 这里AVPacket->pts赋值为timestamp
        // using DecodeCallback = std::function<void(uint64_t pktid, AVFrame *outframe)>;

        virtual int open(AVCodecID codecID, DecodeCallback callback);

        int         decode(uint64_t pktid, AVPacket *inpkt, uint64_t timestamp);
        virtual int decode(uint64_t pktid, FGRecord::AVPacketSP fgpkt);

        virtual int flush(uint64_t ptkid);

        virtual int close();

        virtual AVBufferRef *getHwFramesCtx();

    private:
        const AVCodec  *pDecodec_     = nullptr;
        AVCodecContext *pDecodec_ctx_ = nullptr;

        DecodeCallback callback_ = nullptr;
    };
} // namespace Codec