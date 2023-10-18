#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "decoder.h"

#include <functional>
#include <memory>
#include <string>

namespace Codec {
    class QSVDecoder : public BasicDecoder {
    public:
        explicit QSVDecoder(int chn)
            : BasicDecoder(chn){};
        virtual ~QSVDecoder(){};

        virtual int open(AVCodecID codecID, DecodeCallback callback);

        virtual int decode(uint64_t pktid, FGRecord::AVPacketSP fgpkt);

        virtual int flush(uint64_t ptkid);

        virtual int close();

        virtual AVBufferRef *getHwFramesCtx();

    private:
        const AVCodec  *pDecodec_     = nullptr;
        AVCodecContext *pDecodec_ctx_ = nullptr;

        AVBufferRef *hw_device_ctx_ = nullptr;

        DecodeCallback callback_ = nullptr;
    };
} // namespace NSRTools
