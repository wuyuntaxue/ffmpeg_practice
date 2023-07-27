#include "h264decoder.h"

#include <stdio.h>

#include <iostream>

#define VIDEO_INBUF_SIZE    200000
#define VIDEO_REFILL_THRESH 4096

static char  err_buf[1280] = {0};
static char *av_get_err(int errnum) {
    av_strerror(errnum, err_buf, 128);
    return err_buf;
}

FFMPEGDecoder::FFMPEGDecoder(std::string in, std::string out) {
    init_inAndout_file(in, out);
}

FFMPEGDecoder::~FFMPEGDecoder() {
    deinit_inAndout_file();
}

int FFMPEGDecoder::init_decoder() {

    std::cout << "init decoder" << std::endl;

    // 获取解码器
    pAVCodec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (pAVCodec_ == nullptr) {
        std::cout << "find decoder failed" << std::endl;
        return -1;
    }

    // 初始化上下文
    pAVCodecContext_ = avcodec_alloc_context3(pAVCodec_);
    if (pAVCodecContext_ == nullptr) {
        std::cout << "avcodec_alloc_context3 failed" << std::endl;
        return -1;
    }

    // 将上下文和解码器关联
    int ret = avcodec_open2(pAVCodecContext_, pAVCodec_, NULL);
    if (ret < 0) {
        std::cout << "avcodec_open2 failed" << std::endl;
        return -1;
    }

    // 获取裸流的解析器 AVCodecParserContext(数据)  +  AVCodecParser(方法)
    pParserContext_ = av_parser_init(pAVCodec_->id);
    if (pParserContext_ == nullptr) {
        // fprintf(stderr, "Parser not found\n");
        std::cout << "av parser init failed" << std::endl;
        return -1;
    }


    return 0;
}

int FFMPEGDecoder::deinit_decoder() {

    avcodec_free_context(&pAVCodecContext_);
    av_parser_close(pParserContext_);
    return 0;
}

int FFMPEGDecoder::decode() {

    // 创建AVPacket和AVFrame
    AVPacket *pkt = av_packet_alloc();
    if (pkt == nullptr) {
        return -1;
    }

    // 读取数据并解析
    uint8_t *inbuf     = new uint8_t[VIDEO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data      = inbuf;
    size_t   data_size = 0;
    // 读取VIDEO_INBUF_SIZE大小的数据到缓存区
    data_size = fread(inbuf, 1, VIDEO_INBUF_SIZE, infile_);
    while (data_size > 0) {
        // 从文件中解析出一个完整的Packet
        int ret = av_parser_parse2(pParserContext_, pAVCodecContext_, &pkt->data, &pkt->size, data,
                                   (int)data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (ret < 0) {
            fprintf(stderr, "Error while parsing\n");
            break;
        }
        data += ret;
        data_size -= ret;
        if (pkt->size) {
            // 解码pkt，并写入文件
            transcode(pAVCodecContext_, pkt);
        }

        if (data_size < VIDEO_REFILL_THRESH) {
            memmove(inbuf, data, data_size);
            data       = inbuf;
            size_t len = fread(data + data_size, 1, VIDEO_INBUF_SIZE - data_size, infile_);
            if (len > 0)
                data_size += len;
        }
    }

    av_packet_free(&pkt);
    delete[] inbuf;
    std::cout << "decode done" << std::endl;

    return 0;
}

// AVPacket转换到AVFrame，并写入文件
int FFMPEGDecoder::transcode(AVCodecContext *codec_ctx, AVPacket *pkt) {

    int ret = avcodec_send_packet(codec_ctx, pkt);
    if (ret == AVERROR(EAGAIN)) {
        std::cout << "Receive_frame and send_packet both returned EAGAIN" << std::endl;
    } else if (ret < 0) {
        std::cout << "Error submitting the packet to the decoder" << std::endl;
        return -1;
    }
    AVFrame *frame = av_frame_alloc();

    while (ret >= 0) {
        // 一个packet可能包含多个frame，所以要获取多次
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            std::cout << "decoder failed";
            break;
        }
        {
            static unsigned int count = 0;
            if (count % 100 == 0) {
                printf("width: %u, height: %u, format: %u\n", frame->width, frame->height, frame->format);
            }
            count++;
            if (count > 0xfffffff0) {
                count = 0;
            }
        }
        write_yuv(frame);
    }
    av_frame_free(&frame);
    return 0;
}

int FFMPEGDecoder::write_yuv(AVFrame *frame) {
    for (int j = 0; j < frame->height; j++)
        fwrite(frame->data[0] + j * frame->linesize[0], 1, frame->width, outfile_);
    for (int j = 0; j < frame->height / 2; j++)
        fwrite(frame->data[1] + j * frame->linesize[1], 1, frame->width / 2, outfile_);
    for (int j = 0; j < frame->height / 2; j++)
        fwrite(frame->data[2] + j * frame->linesize[2], 1, frame->width / 2, outfile_);
    return 0;
}


int FFMPEGDecoder::init_inAndout_file(std::string &input, std::string &output) {
    inputFileName_  = input;
    outputFileName_ = output;

    infile_ = fopen(inputFileName_.c_str(), "rb");
    if (infile_ == nullptr) {
        std::cout << "inputfile open failed: " << inputFileName_ << std::endl;
        return -1;
    }
    outfile_ = fopen(outputFileName_.c_str(), "wb");
    if (outfile_ == nullptr) {
        std::cout << "outputfile open failed: " << outputFileName_ << std::endl;
        return -1;
    }
    return 0;
}

int FFMPEGDecoder::deinit_inAndout_file() {
    if (infile_) {
        fclose(infile_);
    }
    if (outfile_) {
        fclose(outfile_);
    }
    return 0;
}
