#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libswscale/swscale.h>
}
#include <string>

//H264文件解码成YUV420P并写入文件

class FFMPEGDecoder {
public:
    FFMPEGDecoder(std::string in, std::string out);
    ~FFMPEGDecoder();

    int init_decoder();
    int deinit_decoder();

    int decode();

private:
    int transcode(AVCodecContext *codec_ctx, AVPacket *pkt);
    int write_yuv(AVFrame *frame);

    int init_inAndout_file(std::string &input, std::string &output);
    int deinit_inAndout_file();

private:
    const AVCodec        *pAVCodec_        = nullptr;
    AVCodecContext       *pAVCodecContext_ = nullptr;
    AVCodecParameters    *pParameters_     = nullptr;
    AVCodecParserContext *pParserContext_  = nullptr;

    std::string inputFileName_  = "test.h264";
    std::string outputFileName_ = "test.yuv";

    FILE *infile_  = nullptr;
    FILE *outfile_ = nullptr;
};

//  ffplay -pixel_format yuv420p -video_size 768x320 -framerate 25 out.yuv

// 参考： https://blog.csdn.net/yinshipin007/article/details/131789217