#include "sw_decoder.h"

extern "C" {
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace Codec {

    static char errStr[1024];

    int SwDecoder::open(AVCodecID codecID, DecodeCallback callback) {

        if (callback == nullptr) {
            LOG_CHN(ERROR, chn_) << "callback is null , init failed";
            return -1;
        }

        pDecodec_ = avcodec_find_decoder(codecID);
        if (pDecodec_ == nullptr) {
            LOG_CHN(ERROR, chn_) << "find decoder failed, " << codecID;
            return -1;
        }

        pDecodec_ctx_ = avcodec_alloc_context3(pDecodec_);
        if (pDecodec_ctx_ == nullptr) {
            LOG_CHN(ERROR, chn_) << "decoder alloc context failed, " << codecID;
            return -1;
        }

        av_opt_set(pDecodec_ctx_->priv_data, "tune", "zerolatency", 0);

        int ret = avcodec_open2(pDecodec_ctx_, pDecodec_, NULL);
        if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            LOG_CHN(ERROR, chn_) << "decoder open failed, " << errStr;
            return -1;
        }

        callback_ = std::move(callback);
        LOG_CHN(INFO, chn_) << "sw decoder init done";
        return 0;
    }

    int SwDecoder::close() {
        if (pDecodec_ctx_) {
            avcodec_close(pDecodec_ctx_);
            avcodec_free_context(&pDecodec_ctx_);
        }
        callback_ = nullptr;
        return 0;
    }

    int SwDecoder::decode(uint64_t pktid, AVPacket *inpkt, uint64_t timestamp) {
        if (inpkt) {
            inpkt->pts = timestamp;
        }
        int ret = avcodec_send_packet(pDecodec_ctx_, inpkt);
        if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            LOG_CHN(ERROR, chn_) << "decoder send packet failed, " << errStr << std::endl;
            return -1;
        }

        AVFrame *frame = av_frame_alloc();
        while (ret >= 0) {
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
        }

        av_frame_free(&frame);
        return 0;
    }

    int SwDecoder::decode(uint64_t pktid, FGRecord::AVPacketSP fgpkt) {
        int ret = 0;
        if (fgpkt) {
            auto     depkt = av_packet_alloc();
            uint8_t *dat   = (uint8_t *)av_malloc(fgpkt->size);
            if (dat == nullptr) {
                LOG_CHN(ERROR, chn_) << "av_malloc error" << std::endl;
                return -1;
            }
            memcpy(dat, fgpkt->data.get(), fgpkt->size);

            ret = av_packet_from_data(depkt, dat, fgpkt->size);
            if (ret < 0) {
                av_strerror(ret, errStr, sizeof(errStr));
                LOG_CHN(ERROR, chn_) << "av_packet_from_data error, " << errStr << std::endl;
                return -1;
            }
            // 为了AVFrame的时间戳能和AVPacket对应
            depkt->pts = fgpkt->timestamp;

            ret = avcodec_send_packet(pDecodec_ctx_, depkt);
            if (ret < 0) {
                av_strerror(ret, errStr, sizeof(errStr));
                LOG_CHN(ERROR, chn_) << "decoder send packet failed, " << errStr << std::endl;
                return -1;
            }
        } else {
            ret = avcodec_send_packet(pDecodec_ctx_, nullptr);
            if (ret < 0) {
                av_strerror(ret, errStr, sizeof(errStr));
                LOG_CHN(ERROR, chn_) << "decoder send packet failed, " << errStr << std::endl;
                return -1;
            }
        }


        AVFrame *frame = av_frame_alloc();
        while (ret >= 0) {
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
        }

        av_frame_free(&frame);
        return 0;
    }

    int SwDecoder::flush(uint64_t ptkid) {
        if (pDecodec_ctx_) {
            decode(ptkid, nullptr, 0);
            avcodec_flush_buffers(pDecodec_ctx_);
        }
        return 0;
    }

    AVBufferRef *SwDecoder::getHwFramesCtx() {
        // 只有硬件编码器有效
        return nullptr;
    }
} // namespace Codec
