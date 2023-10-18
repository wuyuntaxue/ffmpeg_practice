#include "vaapi_encoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/hwcontext_vaapi.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}


namespace Codec {
    static char errStr[1024];

    int VAAPIEncoder::open(AVCodecID codecID, AVPixelFormat inPixel, AVBufferRef *hw_frame_ctx, int width,
                           int height, EncodeCallback callback) {

        if (encoder_opened_) {
            LOG_CHN(ERROR, chn_) << "encoder already open";
            return -1;
        }
        std::string enc_name;
        switch (codecID) {
        case AV_CODEC_ID_H264:
            enc_name = "h264_vaapi";
            break;
        case AV_CODEC_ID_H265:
            enc_name = "hevc_vaapi";
            break;
        default:
            LOG_CHN(ERROR, chn_) << "unsupport code id, " << codecID;
            return -1;
        }
        hw_format_      = AV_PIX_FMT_VAAPI;
        hw_device_type_ = AV_HWDEVICE_TYPE_VAAPI;
        int ret         = 0;

        do {
            // find encoder
            encodec_ = avcodec_find_encoder_by_name(enc_name.c_str());
            if (encodec_ == nullptr) {
                LOG_CHN(ERROR, chn_) << "encoder not find, " << enc_name;
                break;
            }

            // encoder ctx alloc and init
            encodec_ctx_ = avcodec_alloc_context3(encodec_);
            if (encodec_ctx_ == nullptr) {
                LOG_CHN(ERROR, chn_) << "codec ctx alloc failed";
                break;
            }

            if (hw_frame_ctx == nullptr) {
                // frame must is software data
                // hw device create
                ret = av_hwdevice_ctx_create(&device_ctx_ref_, hw_device_type_, NULL, NULL, 0);
                if (ret < 0) {
                    av_strerror(ret, errStr, sizeof(errStr));
                    LOG_CHN(ERROR, chn_) << "hwdevice ctx create failed, " << errStr;
                    break;
                }

                // hw frame ctx alloc and init
                hw_frames_ctx_ref_ = av_hwframe_ctx_alloc(device_ctx_ref_);
                if (hw_frames_ctx_ref_ == nullptr) {
                    av_strerror(ret, errStr, sizeof(errStr));
                    LOG_CHN(ERROR, chn_) << "av_hwframe_ctx_alloc failed, " << errStr;
                    break;
                }

                AVHWFramesContext *hw_frames_ctx = (AVHWFramesContext *)hw_frames_ctx_ref_->data;
                hw_frames_ctx->format            = hw_format_;
                hw_frames_ctx->sw_format         = inPixel;
                hw_frames_ctx->width             = width;
                hw_frames_ctx->height            = height;
                hw_frames_ctx->initial_pool_size = 1;

                ret = av_hwframe_ctx_init(hw_frames_ctx_ref_);
                if (ret < 0) {
                    av_strerror(ret, errStr, sizeof(errStr));
                    LOG_CHN(ERROR, chn_) << "av_hwframe_ctx_init failed, " << errStr;
                    break;
                }
                encodec_ctx_->hw_frames_ctx = av_buffer_ref(hw_frames_ctx_ref_);
            } else {
                encodec_ctx_->hw_frames_ctx = av_buffer_ref(hw_frame_ctx);
            }
            encodec_ctx_->width     = width;
            encodec_ctx_->height    = height;
            encodec_ctx_->time_base = AVRational{1, 25};
            encodec_ctx_->framerate = AVRational{25, 1};
            // encodec_ctx_->global_quality = 50; //编码器全局质量
            encodec_ctx_->pix_fmt      = hw_format_;
            encodec_ctx_->gop_size     = 30;
            encodec_ctx_->max_b_frames = 0;
            // encodec_ctx_->has_b_frames = 0;

            av_opt_set(encodec_ctx_->priv_data, "tune", "zerolatency", 0);

            // open encodec
            ret = avcodec_open2(encodec_ctx_, encodec_, NULL);
            if (ret < 0) {
                av_strerror(ret, errStr, sizeof(errStr));
                LOG_CHN(ERROR, chn_) << "avcodec_open2 failed, " << errStr;
                break;
            }

            encoder_opened_ = true;
            callback_       = std::move(callback);
            LOG_CHN(INFO, chn_) << enc_name << " encoder init done";
            return 0;
        } while (0);

        if (encodec_ctx_) {
            avcodec_free_context(&encodec_ctx_);
            encodec_ctx_ = nullptr;
        }

        if (hw_frames_ctx_ref_) {
            av_buffer_unref(&hw_frames_ctx_ref_);
            hw_frames_ctx_ref_ = nullptr;
        }

        if (device_ctx_ref_) {
            av_buffer_unref(&device_ctx_ref_);
            device_ctx_ref_ = nullptr;
        }

        return -1;
    }

    int VAAPIEncoder::encode(uint64_t frameid, AVFrame *inframe) {
        int ret = avcodec_send_frame(encodec_ctx_, inframe);
        if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            LOG_CHN(ERROR, chn_) << "Error during encoding. Error code: " << errStr;
            return ret;
        }

        AVPacket *out_pkt = av_packet_alloc();
        while (ret >= 0) {
            ret = avcodec_receive_packet(encodec_ctx_, out_pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                av_strerror(ret, errStr, sizeof(errStr));
                LOG_CHN(ERROR, chn_) << "encoder receive packet failed, " << errStr;
                break;
            }

            if (callback_) {
                if (inframe) {
                    out_pkt->pts = inframe->pts;
                }
                callback_(frameid, out_pkt);
            }
        }
        if (out_pkt) {
            av_packet_free(&out_pkt);
        }
        return 0;
    }

    int VAAPIEncoder::flush(uint64_t frameid) {
        if (encodec_ctx_) {
            encode(frameid, nullptr);
            avcodec_flush_buffers(encodec_ctx_);
        }
        return 0;
    }

    int VAAPIEncoder::close() {
        if (encodec_ctx_ != nullptr) {
            avcodec_close(encodec_ctx_);
            avcodec_free_context(&encodec_ctx_);
        }
        encoder_opened_ = false;
        return 0;
    }

    bool VAAPIEncoder::isopend() {
        return encoder_opened_;
    }
} // namespace Codec