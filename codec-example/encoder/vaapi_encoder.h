#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}
#include "encoder.h"

#include <functional>
#include <string>


namespace Codec {
    class VAAPIEncoder : public BasicEncoder {
    public:
        explicit VAAPIEncoder(int chn)
            : BasicEncoder(chn){};
        virtual ~VAAPIEncoder(){};

        virtual int open(AVCodecID codecID, AVPixelFormat inPixel, AVBufferRef *hw_frame_ctx, int width,
                         int height, EncodeCallback callback);

        virtual int encode(uint64_t frameid, AVFrame *inframe);

        virtual int flush(uint64_t frameid);

        virtual int close();

        virtual bool isopend();

    private:
        // encoder
        const AVCodec  *encodec_           = nullptr;
        AVCodecContext *encodec_ctx_       = nullptr;
        AVBufferRef    *device_ctx_ref_    = nullptr;
        AVBufferRef    *hw_frames_ctx_ref_ = nullptr;

        bool           encoder_opened_ = false;
        EncodeCallback callback_       = nullptr;

        AVPixelFormat  hw_format_      = AV_PIX_FMT_NONE;
        AVHWDeviceType hw_device_type_ = AV_HWDEVICE_TYPE_NONE;
    };
} // namespace Codec