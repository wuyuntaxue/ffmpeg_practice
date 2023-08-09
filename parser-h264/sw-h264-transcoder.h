#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <string>
#include <functional>
#include <memory>

/**
 * @brief 转码视频裸流文件
 * 
 */

class SWTranscoder {
public:
    SWTranscoder();
    ~SWTranscoder();

    using DecodeCallback = std::function<void(AVFrame *outframe)>;
    using EncodeCallback = std::function<void(AVPacket *outpkt)>;

    int open(AVCodecID inCodecID, AVCodecID outCodecID);

    int transcode(std::string infile, std::string outfile);

    int close();

private:
    int open_decoder(AVCodecID codecID, AVPixelFormat outPixel);
    int open_encoder(AVCodecID codecID, AVPixelFormat inPixel, int width, int height);

    int do_decode(AVPacket *inpkt, DecodeCallback callback);
    int do_encode(AVFrame *inframe, EncodeCallback callback);

    int close_decoder();
    int close_encoder();

    int open_parser(AVCodecID codecID);
    int close_parser();

private:
    // decoder
    const AVCodec  *pDecodec_     = nullptr;
    AVCodecContext *pDecodec_ctx_ = nullptr;

    // encoder
    const AVCodec  *pEncodec_     = nullptr;
    AVCodecContext *pEncodec_ctx_ = nullptr;

    // parser
    AVCodecParserContext *pParser_ctx_ = nullptr;

    // 中转的像素格式
    const AVPixelFormat transit_pixel_ = AV_PIX_FMT_YUV420P;

    bool encoder_opened_ = false;
    AVCodecID out_codec_id_ = AV_CODEC_ID_NONE;
};