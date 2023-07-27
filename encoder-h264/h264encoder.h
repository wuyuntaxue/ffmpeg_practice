#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libswscale/swscale.h>
}

#include <functional>

class FFMPEGEncoder {
public:
    FFMPEGEncoder();
    ~FFMPEGEncoder();

    FFMPEGEncoder(const FFMPEGEncoder &) = delete;

    int init_encoder();
    int deinit_encoder();

    int working(std::string infilename, std::string outfilename);

    int transcode(AVFrame *frame, std::function<void(AVPacket *pkt)> callback);

    // 从YUV文件中读取AVFrame，传入回调函数
    int read_yuv_file(std::string filename, int width, int height,
                      std::function<void(AVFrame *frame)> callback);

private:
    const AVCodec  *pAVCodec_        = nullptr;
    AVCodecContext *pAVCodecContext_ = nullptr;
};
