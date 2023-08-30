#include "sw-h264-transcoder.h"

extern "C" {
#include <libavutil/log.h>
#include <libswscale/swscale.h>
}

#include <cstdio>
#include <iostream>

#define VIDEO_INBUF_SIZE    200000
#define VIDEO_REFILL_THRESH 4096

static char errStr[1024];

static enum AVPixelFormat get_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    (void)ctx;
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == AV_PIX_FMT_RGB24) {
            std::cout << "success get piexl fmt" << std::endl;
            return *p;
        }
    }
    return AV_PIX_FMT_NONE;
}

SWTranscoder::SWTranscoder() {}
SWTranscoder::~SWTranscoder() {}

int SWTranscoder::open(AVCodecID inCodecID, AVCodecID outCodecID) {
    av_log_set_level(AV_LOG_DEBUG);

    if (0 != open_decoder(inCodecID, transit_pixel_)) {
        return -1;
    }

    // if (0 != open_encoder(outCodecID, transit_pixel_)) {
    //     return -1;
    // }
    out_codec_id_ = outCodecID;

    std::cout << "init done" << std::endl;
    return 0;
}

int SWTranscoder::transcode(std::string infile, std::string outfile) {

    int ret = open_format(infile);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "find stream format failed" << std::endl;
        return -1;
    }

    FILE *pOutFile = fopen(outfile.c_str(), "w");
    if (pOutFile == nullptr) {
        std::cout << outfile << " open failed" << std::endl;
        return -1;
    }

    AVPacket    *pkt      = av_packet_alloc();
    unsigned int pktCount = 0;

    while (av_read_frame(pFormat_ctx_, pkt) >= 0) {

        if (pkt->size && pkt->stream_index == video_index_) {
            do_decode(pktCount, pkt, [&](unsigned int pktid, AVFrame *frame) {
                std::cout << "decode callback " << pktid << std::endl;

                if (!encoder_opened_) {
                    open_encoder(out_codec_id_, (AVPixelFormat)frame->format, pDecodec_ctx_->width,
                                 pDecodec_ctx_->height);
                }

                if (encoder_opened_) {
                    do_encode(pktid, frame, [&](unsigned int frameid, AVPacket *outpkt) {
                        std::cout << "encode callback " << frameid << std::endl;
                        fwrite(outpkt->data, 1, outpkt->size, pOutFile);
                    });
                }
            });
        }
        if (encoder_opened_) {
            close_encoder();
        }
        pktCount++;
    }

    av_packet_free(&pkt);
    fclose(pOutFile);
    return 0;
}

int SWTranscoder::close() {
    close_encoder();
    close_format();
    close_decoder();
    return 0;
}

///////////////////////////////////////////////////////////////////

// decoder
int SWTranscoder::open_decoder(AVCodecID codecID, AVPixelFormat outPixel) {
    pDecodec_ = avcodec_find_decoder(codecID);
    if (pDecodec_ == nullptr) {
        std::cout << "find decoder failed, " << codecID << std::endl;
        return -1;
    }

    pDecodec_ctx_ = avcodec_alloc_context3(pDecodec_);
    if (pDecodec_ctx_ == nullptr) {
        std::cout << "decoder alloc context failed, " << codecID << std::endl;
        return -1;
    }

    // todo 设置像素格式没有作用
    pDecodec_ctx_->pix_fmt    = outPixel;
    pDecodec_ctx_->sw_pix_fmt = outPixel;
    // pDecodec_ctx_->get_format = get_format;

    int ret = avcodec_open2(pDecodec_ctx_, pDecodec_, NULL);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "decoder open failed, " << errStr << std::endl;
        return -1;
    }
    std::cout << "decoder init done." << std::endl;
    return 0;
}

int SWTranscoder::close_decoder() {
    if (pDecodec_ctx_) {
        avcodec_close(pDecodec_ctx_);
        avcodec_free_context(&pDecodec_ctx_);
    }
    return 0;
}

int SWTranscoder::do_decode(unsigned int pktid, AVPacket *inpkt, DecodeCallback callback) {
    int ret = avcodec_send_packet(pDecodec_ctx_, inpkt);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "decoder send packet failed, " << errStr << std::endl;
        return -1;
    }

    AVFrame *frame = av_frame_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_frame(pDecodec_ctx_, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            std::cout << "decoder failed, " << errStr << std::endl;
            break;
        }
        if (callback) {
            callback(pktid, frame);
        }
    }

    av_frame_free(&frame);
    return 0;
}

// encoder
int SWTranscoder::open_encoder(AVCodecID codecID, AVPixelFormat inPixel, int width, int height) {
    pEncodec_ = avcodec_find_encoder(codecID);
    if (pEncodec_ == nullptr) {
        std::cout << "find encoder failed, " << codecID << std::endl;
        return -1;
    }

    pEncodec_ctx_ = avcodec_alloc_context3(pEncodec_);
    if (pEncodec_ctx_ == nullptr) {
        std::cout << "encoder alloc context failed, " << codecID << std::endl;
        return -1;
    }

    pEncodec_ctx_->pix_fmt               = inPixel;
    pEncodec_ctx_->width                 = width;
    pEncodec_ctx_->height                = height;
    pEncodec_ctx_->time_base             = AVRational{1, 25}; // TODO
    pEncodec_ctx_->framerate             = AVRational{25, 1};
    pEncodec_ctx_->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;
    pEncodec_ctx_->gop_size              = 50;

    int ret = avcodec_open2(pEncodec_ctx_, pEncodec_, NULL);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "encoder open failed, " << errStr << std::endl;
        return -1;
    }
    encoder_opened_ = true;
    std::cout << "encoder init done." << std::endl;
    return 0;
}

int SWTranscoder::close_encoder() {
    encoder_opened_ = false;
    if (pEncodec_ctx_) {
        avcodec_close(pEncodec_ctx_);
        avcodec_free_context(&pEncodec_ctx_);
    }
    return 0;
}

int SWTranscoder::do_encode(unsigned int frameid, AVFrame *inframe, EncodeCallback callback) {
    int ret = avcodec_send_frame(pEncodec_ctx_, inframe);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "encoder send frame failed, " << errStr << std::endl;
        return -1;
    }

    AVPacket *pkt = av_packet_alloc();

    while (ret >= 0) {
        ret = avcodec_receive_packet(pEncodec_ctx_, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            std::cout << "encoder receive packet failed, " << errStr << std::endl;
            break;
        }

        if (callback) {
            callback(frameid, pkt);
        }
    }

    av_packet_free(&pkt);
    return 0;
}

int SWTranscoder::open_format(std::string filename) {

    do {
        // 打开文件，如果是url则创建网络链接
        int ret = avformat_open_input(&pFormat_ctx_, filename.c_str(), NULL, NULL);
        if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            std::cout << "format open failed, " << errStr << std::endl;
            break;
        }

        // 读取码流信息到 avformat_ctx
        ret = avformat_find_stream_info(pFormat_ctx_, NULL);
        if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            std::cout << "find stream info failed, " << errStr << std::endl;
            break;
        }

        // 找出视频流
        video_index_ = av_find_best_stream(pFormat_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (video_index_ < 0) {
            av_strerror(video_index_, errStr, sizeof(errStr));
            std::cout << "find best video stream failed, " << errStr << std::endl;
            break;
        } else {
            return 0;
        }

    } while (0);

    if (pFormat_ctx_) {
        avformat_close_input(&pFormat_ctx_);
    }
    return -1;
}

int SWTranscoder::close_format() {
    if (pFormat_ctx_) {
        avformat_close_input(&pFormat_ctx_);
    }
    return 0;
}