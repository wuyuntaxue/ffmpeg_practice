#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}
#include <functional>
#include <memory>
#include <string>
#include "common.hpp"

namespace Codec {
    class BasicEncoder {
    public:
        using Ptr = std::shared_ptr<BasicEncoder>;

        explicit BasicEncoder(int chn)
            : chn_(chn){};
        virtual ~BasicEncoder(){};

        using EncodeCallback = std::function<void(uint64_t frameid, AVPacket *outpkt)>;

        virtual int open(AVCodecID codecID, AVPixelFormat inPixel, AVBufferRef *hw_frame_ctx, int width,
                         int height, EncodeCallback callback) = 0;

        virtual int encode(uint64_t frameid, AVFrame *inframe) = 0;

        virtual int flush(uint64_t frameid) = 0;

        virtual int close() = 0;

        virtual bool isopend() = 0;

    protected:
        int chn_ = 1;
    };

} // namespace Codec