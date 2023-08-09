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

    if (0 != open_parser(inCodecID)) {
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
    FILE *pInFile = fopen(infile.c_str(), "r");
    if (pInFile == nullptr) {
        std::cout << infile << " open failed" << std::endl;
        return -1;
    }

    FILE *pOutFile = fopen(outfile.c_str(), "w");
    if (pOutFile == nullptr) {
        std::cout << outfile << " open failed" << std::endl;
        fclose(pInFile);
        return -1;
    }

    uint8_t  *inbuf = new uint8_t[VIDEO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t  *data  = inbuf;
    AVPacket *pkt   = av_packet_alloc();

    size_t data_size = fread(inbuf, 1, VIDEO_INBUF_SIZE, pInFile);
    while (data_size > 0) {
        int ret = av_parser_parse2(pParser_ctx_, pDecodec_ctx_, &pkt->data, &pkt->size, data, (int)data_size,
                                   AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            std::cout << "parse failed, " << errStr << std::endl;
            break;
        }
        data += ret;
        data_size -= ret;
        if (data_size < VIDEO_REFILL_THRESH) {
            memmove(inbuf, data, data_size);
            data       = inbuf;
            size_t len = fread(data + data_size, 1, VIDEO_INBUF_SIZE - data_size, pInFile);
            if (len > 0) {
                data_size += len;
            }
        }

        if (pkt->size) {
            do_decode(pkt, [&](AVFrame *frame) {
                // std::cout << "decode callback" << std::endl;
                if (!encoder_opened_) {
                    open_encoder(out_codec_id_, (AVPixelFormat)frame->format, pDecodec_ctx_->width,
                                 pDecodec_ctx_->height);
                }
                if (encoder_opened_) {
                    do_encode(frame, [&](AVPacket *outpkt) {
                        // std::cout << "encode callback" << std::endl;
                        static int count = 0;
                        if (count == 0)
                            fwrite(outpkt->data, 1, outpkt->size, pOutFile);
                        count++;
                    });
                }
            });
        }
    }

    av_packet_free(&pkt);
    delete[] inbuf;
    fclose(pOutFile);
    fclose(pInFile);
    return 0;
}

int SWTranscoder::close() {
    close_encoder();
    close_parser();
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

    //todo 设置像素格式没有作用
    pDecodec_ctx_->pix_fmt = outPixel;
    pDecodec_ctx_->sw_pix_fmt = outPixel;
    // pDecodec_ctx_->get_format = get_format;

    int ret = avcodec_open2(pDecodec_ctx_, pDecodec_, NULL);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "decoder open failed, " << errStr << std::endl;
        return -1;
    }
    return 0;
}

int SWTranscoder::close_decoder() {
    if (pDecodec_ctx_) {
        avcodec_close(pDecodec_ctx_);
        avcodec_free_context(&pDecodec_ctx_);
    }
    return 0;
}

int SWTranscoder::do_decode(AVPacket *inpkt, DecodeCallback callback) {
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
            callback(frame);
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

    int ret = avcodec_open2(pEncodec_ctx_, pEncodec_, NULL);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "encoder open failed, " << errStr << std::endl;
        return -1;
    }
    encoder_opened_ = true;
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

int SWTranscoder::do_encode(AVFrame *inframe, EncodeCallback callback) {
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
            callback(pkt);
        }
    }

    av_packet_free(&pkt);
    return 0;
}

// parser
int SWTranscoder::open_parser(AVCodecID codecID) {
    // 获取裸流的解析器
    pParser_ctx_ = av_parser_init(codecID);
    if (pParser_ctx_ == nullptr) {
        std::cout << "av parser init failed" << std::endl;
        return -1;
    }
    return 0;
}

int SWTranscoder::close_parser() {
    if (pParser_ctx_) {
        av_parser_close(pParser_ctx_);
    }
    return 0;
}