extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/hwcontext_vaapi.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

#include "vaapi_decoder.h"

namespace Codec {

    static char errStr[1024];

    static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
        const enum AVPixelFormat *p;
        (void)ctx;
        for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
            if (*p == AV_PIX_FMT_VAAPI) {
                LOG(INFO) << "Success to get HW surface format.";
                return *p;
            }
        }
        LOG(ERROR) << "Failed to get HW surface format.";
        return AV_PIX_FMT_NONE;
    }

    int VAAPIDecoder::open(AVCodecID codecID, DecodeCallback callback) {
        int                 ret;
        enum AVHWDeviceType device_type = AV_HWDEVICE_TYPE_NONE;

        do {
            const char *device_name = "vaapi";
            device_type             = av_hwdevice_find_type_by_name(device_name);
            if (device_type == AV_HWDEVICE_TYPE_VAAPI) {
                // LOG_CHN(INFO, chn_) << "find hwdevice AV_HWDEVICE_TYPE_VAAPI";
            } else {
                LOG_CHN(ERROR, chn_) << "can't find hwdevice AV_HWDEVICE_TYPE_VAAPI";
                LOG_CHN(ERROR, chn_) << "Available device types:";
                while ((device_type = av_hwdevice_iterate_types(device_type)) != AV_HWDEVICE_TYPE_NONE)
                    LOG_CHN(ERROR, chn_) << av_hwdevice_get_type_name(device_type);
                return -1;
            }

            // --------------------------- 2. create h264 decode
            pDecodec_ = avcodec_find_decoder((AVCodecID)codecID);
            if (!pDecodec_) {
                LOG_CHN(ERROR, chn_) << "ffmpeg decode, Codec AV_CODEC_ID_H264 not found";
                return -1;
            }
            AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;
            for (int i = 0;; i++) {

                const AVCodecHWConfig *config = avcodec_get_hw_config(pDecodec_, i);
                if (!config) {
                    LOG_CHN(ERROR, chn_)
                        << "Decoder " << pDecodec_->name << " does not support device type " << device_name;
                    return -1;
                }

                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                    config->device_type == device_type) {
                    hw_pix_fmt = config->pix_fmt;
                    // LOG_CHN(INFO, chn_) << "hw_pix_fmt = " << av_get_pix_fmt_name(hw_pix_fmt);
                    break;
                }
            }

            // --------------------------- 3. hardware decode context
            // 给解码器的 Ctx 分配内存...
            if (!(pDecodec_ctx_ = avcodec_alloc_context3(pDecodec_))) {
                LOG_CHN(ERROR, chn_) << "Failed to alloc context.";
                return -1;
            }

            // 设置 hw_device_ctx
            // 输入硬件 ctx 和解码 tpye 初始化硬件，获取硬解码设备的 context
            if ((ret = av_hwdevice_ctx_create(&hw_device_ctx_, device_type, NULL, NULL, 0)) < 0) {
                av_strerror(ret, errStr, sizeof(errStr));
                LOG_CHN(ERROR, chn_) << "Failed to create specified HW device. " << errStr;
                break;
            }

            // 4. set decode para
            AVCodecParameters *par = avcodec_parameters_alloc();
            if (par == nullptr) {
                LOG_CHN(ERROR, chn_) << "video AVCodecParameters alloc failed ";
                break;
            }
            // par->width       = 1920;
            // par->height      = 1080;
            par->codec_type  = AVMEDIA_TYPE_VIDEO;
            par->codec_id    = (AVCodecID)codecID;
            par->format      = AV_PIX_FMT_YUV420P; // AV_PIX_FMT_NV12
            par->color_range = AVCOL_RANGE_JPEG;
            par->video_delay = 0;


            avcodec_parameters_to_context(pDecodec_ctx_, par);
            avcodec_parameters_free(&par);

            pDecodec_ctx_->framerate     = AVRational{.num = 25, .den = 1};
            pDecodec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
            pDecodec_ctx_->time_base     = av_inv_q(pDecodec_ctx_->framerate);
            pDecodec_ctx_->get_format    = get_hw_format;
            // 90000 Hz is the default timebase for video stream sent via RTSP
            pDecodec_ctx_->pkt_timebase = AVRational{.num = 1, .den = 90000};
            pDecodec_ctx_->pix_fmt      = hw_pix_fmt;
            av_opt_set(pDecodec_ctx_->priv_data, "tune", "zerolatency", 0);

            if ((ret = avcodec_open2(pDecodec_ctx_, pDecodec_, NULL)) < 0) {
                av_strerror(ret, errStr, sizeof(errStr));
                LOG_CHN(ERROR, chn_) << "Failed to open codec: " << errStr;
                break;
            }

        } while (0);

        callback_ = std::move(callback);
        LOG_CHN(INFO, chn_) << "vaapi decoder init done";
        return ret;
    }

    int VAAPIDecoder::decode(uint64_t pktid, FGRecord::AVPacketSP fgpkt) {
        AVPacket *packet = nullptr;
        if (fgpkt) {
            packet = av_packet_alloc();
            if (NULL == packet) {
                LOG_CHN(ERROR, chn_) << "av_packet_alloc fail.";
                return -1;
            }
            packet->size = fgpkt->size;
            packet->data = !fgpkt->size ? NULL : fgpkt->data.get();
            packet->pts  = fgpkt->timestamp;
        }

        int ret = avcodec_send_packet(pDecodec_ctx_, packet);
        if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            LOG_CHN(ERROR, chn_) << "avcodec_send_packet err: " << errStr;
            return -1;
        }

        AVFrame *frame = av_frame_alloc();
        if (NULL == frame) {
            return -1;
        }

        do {
            ret = avcodec_receive_frame(pDecodec_ctx_, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0) {
                av_strerror(ret, errStr, sizeof(errStr));
                LOG_CHN(ERROR, chn_) << "decoder failed, " << errStr << std::endl;
                break;
            }
            if (callback_) {
                callback_(pktid, frame);
            }

        } while (ret >= 0);
        av_frame_free(&frame);

        return ret;
    }

    int VAAPIDecoder::flush(uint64_t ptkid) {
        if (pDecodec_ctx_) {
            decode(ptkid, nullptr);
            avcodec_flush_buffers(pDecodec_ctx_);
        }
        return 0;
    }

    int VAAPIDecoder::close() {
        if (pDecodec_ctx_) {
            avcodec_close(pDecodec_ctx_);
            avcodec_free_context(&pDecodec_ctx_);
        }
        callback_ = nullptr;
        return 0;
    }

    AVBufferRef *VAAPIDecoder::getHwFramesCtx() {
        if (pDecodec_ctx_) {
            return pDecodec_ctx_->hw_frames_ctx;
        }
        return nullptr;
    }
} // namespace Codec