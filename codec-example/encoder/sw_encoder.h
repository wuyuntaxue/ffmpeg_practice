#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}
#include "encoder.h"

#include <functional>
#include <string>

/**
 * @brief 编码器-软件
 *
 */
namespace Codec {
    class SwEncoder : public BasicEncoder {
    public:
        explicit SwEncoder(int chn)
            : BasicEncoder(chn){};
        virtual ~SwEncoder(){};

        using EncodeCallback = std::function<void(uint64_t frameid, AVPacket *outpkt)>;

        virtual int open(AVCodecID codecID, AVPixelFormat inPixel, AVBufferRef *hw_frame_ctx, int width,
                         int height, EncodeCallback callback);

        virtual int encode(uint64_t frameid, AVFrame *inframe);

        virtual int flush(uint64_t frameid);

        virtual int close();

        virtual bool isopend();

    private:
        // encoder
        const AVCodec  *pEncodec_     = nullptr;
        AVCodecContext *pEncodec_ctx_ = nullptr;

        bool           encoder_opened_ = false;
        EncodeCallback callback_       = nullptr;
    };
} // namespace Codec