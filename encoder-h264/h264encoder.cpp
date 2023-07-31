#include "h264encoder.h"

#include <stdio.h>

#include <iostream>

static char  err_buf[1280] = {0};
static char *av_get_err(int errnum) {
    av_strerror(errnum, err_buf, 128);
    return err_buf;
}

FFMPEGEncoder::FFMPEGEncoder() {}

FFMPEGEncoder::~FFMPEGEncoder() {}

int FFMPEGEncoder::init_encoder() {

    pAVCodec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (pAVCodec_ == nullptr) {
        std::cout << "can't find avcodec" << std::endl;
        return -1;
    }

    pAVCodecContext_ = avcodec_alloc_context3(pAVCodec_);
    if (pAVCodecContext_ == nullptr) {
        std::cout << "avcodec_alloc_context3 failed" << std::endl;
        return -1;
    }

    pAVCodecContext_->time_base = AVRational{1, 25};
    pAVCodecContext_->framerate = AVRational{25, 1};
    pAVCodecContext_->gop_size  = 60; // 关键帧间隔？

    // pAVCodecContext_->width   = 1920;
    // pAVCodecContext_->height  = 1080;
    // pAVCodecContext_->pix_fmt = AV_PIX_FMT_YUV420P;
    return 0;
}

int FFMPEGEncoder::deinit_encoder() {

    avcodec_free_context(&pAVCodecContext_);

    return 0;
}

int FFMPEGEncoder::transcode(AVFrame *frame, std::function<void(AVPacket *pkt)> callback) {
    pAVCodecContext_->width   = frame->width;
    pAVCodecContext_->height  = frame->height;
    pAVCodecContext_->pix_fmt = (AVPixelFormat)frame->format;
    // pAVCodecContext_->pix_fmt = AV_PIX_FMT_YUV420P;
    pAVCodecContext_->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;

    int ret = avcodec_open2(pAVCodecContext_, pAVCodec_, NULL);
    if (ret < 0) {
        std::cout << "avcodec_open2 failed, " << av_get_err(ret) << std::endl;
        return -1;
    }

    ret = avcodec_send_frame(pAVCodecContext_, frame);
    if (ret < 0) {
        std::cout << "Error sending a frame for encoding, " << av_get_err(ret) << std::endl;
        return -1;
    }

    AVPacket *pkt = av_packet_alloc();

    while (ret == 0) {

        ret = avcodec_receive_packet(pAVCodecContext_, pkt);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            fprintf(stderr, "Error encoding video frame\n");
            return -1;
        }

        if (ret == 0 && callback) {
            // fwrite(pkt->data, 1, pkt->size, outFile);
            callback(pkt);
        }
    }

    av_packet_free(&pkt);

    return 0;
}


int FFMPEGEncoder::read_yuv_file(std::string filename, int width, int height,
                                 std::function<void(AVFrame *frame)> callback) {

    FILE *pFile = fopen(filename.c_str(), "rb");
    if (pFile == nullptr) {
        std::cout << "open file[" << filename << "] failed" << std::endl;
        return -1;
    }
    std::cout << "start read file: " << filename << ", width: " << width << ", height: " << height;

    AVFrame *frame = av_frame_alloc();
    frame->width   = width;
    frame->height  = height;
    frame->format  = AV_PIX_FMT_YUV420P;
    av_frame_get_buffer(frame, 0);

    while (!feof(pFile)) {
        int ret = av_frame_is_writable(frame);
        if (ret < 0) {
            av_frame_make_writable(frame);
        }

        fread(frame->data[0], 1, width * height, pFile);     // y
        fread(frame->data[1], 1, width * height / 4, pFile); // u
        fread(frame->data[2], 1, width * height / 4, pFile); // v

        frame->pts += 1000 / (pAVCodecContext_->time_base.den / pAVCodecContext_->time_base.num);
        if (callback) {
            callback(frame);
        }
    }
    if (callback) {
        callback(frame);
    }

    av_frame_free(&frame);
    fclose(pFile);
    return 0;
}

int FFMPEGEncoder::working(std::string infilename, std::string outfilename) {

    FILE *outFile = fopen(outfilename.c_str(), "wb");
    if (outFile == nullptr) {
        std::cout << "open out file failed: " << outfilename << std::endl;
        return -1;
    }
    std::cout << "before read yuv" << std::endl;

    read_yuv_file(infilename, 1920, 1080, [=](AVFrame *frame) {
        int ret = this->transcode(frame, [=](AVPacket *pkt) { fwrite(pkt->data, 1, pkt->size, outFile); });
        if (ret < 0) {
            exit(1);
        }
    });

    fclose(outFile);

    return 0;
}