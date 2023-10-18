#include "sw_encoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace Codec {

    static char errStr[1024];


    int SwEncoder::open(AVCodecID codecID, AVPixelFormat inPixel, AVBufferRef *hw_frame_ctx, int width,
                        int height, EncodeCallback callback) {
        if (callback == nullptr) {
            LOG_CHN(ERROR, chn_) << "callback is null, init failed";
            return -1;
        }
        pEncodec_ = avcodec_find_encoder(codecID);
        if (pEncodec_ == nullptr) {
            LOG_CHN(ERROR, chn_) << "find encoder failed, " << codecID;
            return -1;
        }

        pEncodec_ctx_ = avcodec_alloc_context3(pEncodec_);
        if (pEncodec_ctx_ == nullptr) {
            LOG_CHN(ERROR, chn_) << "encoder alloc context failed, " << codecID;
            return -1;
        }

        pEncodec_ctx_->pix_fmt               = inPixel;
        pEncodec_ctx_->width                 = width;
        pEncodec_ctx_->height                = height;
        pEncodec_ctx_->time_base             = AVRational{1, 25};
        pEncodec_ctx_->framerate             = AVRational{30, 1};
        pEncodec_ctx_->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;
        pEncodec_ctx_->gop_size              = 30;
        pEncodec_ctx_->max_b_frames          = 0;
        pEncodec_ctx_->has_b_frames          = 0;

        // 不缓存帧
        av_opt_set(pEncodec_ctx_->priv_data, "tune", "zerolatency", 0);
        // av_opt_set(pEncodec_ctx_->priv_data, "preset", "superfast", 0);

        int ret = avcodec_open2(pEncodec_ctx_, pEncodec_, NULL);
        if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            LOG_CHN(ERROR, chn_) << "encoder open failed, " << errStr;
            return -1;
        }
        callback_       = std::move(callback);
        encoder_opened_ = true;
        LOG_CHN(INFO, chn_) << "sw encoder init done";
        return 0;
    }

    int SwEncoder::close() {
        encoder_opened_ = false;
        if (pEncodec_ctx_) {
            avcodec_close(pEncodec_ctx_);
            avcodec_free_context(&pEncodec_ctx_);
        }
        return 0;
    }

    int SwEncoder::encode(uint64_t frameid, AVFrame *inframe) {

        int ret = avcodec_send_frame(pEncodec_ctx_, inframe);
        if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            LOG_CHN(ERROR, chn_) << "encoder send frame failed, " << errStr;
            return -1;
        }

        AVPacket *pkt = av_packet_alloc();

        while (ret >= 0) {
            ret = avcodec_receive_packet(pEncodec_ctx_, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                av_strerror(ret, errStr, sizeof(errStr));
                LOG_CHN(ERROR, chn_) << "encoder receive packet failed, " << errStr;
                break;
            }

            if (callback_) {
                callback_(frameid, pkt);
            }
        }

        av_packet_free(&pkt);
        return 0;
    }

    int SwEncoder::flush(uint64_t frameid) {
        if (pEncodec_ctx_) {
            encode(frameid, nullptr);
            avcodec_flush_buffers(pEncodec_ctx_);
        }
        return 0;
    }

    bool SwEncoder::isopend() {
        return encoder_opened_;
    }
} // namespace Codec