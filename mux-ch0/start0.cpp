
extern "C" {
#include <libavformat/avformat.h>
}
#include <iostream>

static char errStr[1024] = {0};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << argv[0] << " filename" << std::endl;
        return -1;
    }
    std::string filename(argv[1]);

    av_log_set_level(AV_LOG_DEBUG);

    AVFormatContext *pFormatCtx = nullptr;

    // 解复用，从文件/网络流中分离出视频/音频流
    do {
        // 打开文件，如果是url则创建网络链接
        int ret = avformat_open_input(&pFormatCtx, filename.c_str(), NULL, NULL);
        if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            std::cout << "format open failed, " << errStr << std::endl;
            break;
        }

        // 读取码流信息到 avformat_ctx
        ret = avformat_find_stream_info(pFormatCtx, NULL);
        if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            std::cout << "find stream info failed, " << errStr << std::endl;
            break;
        }

        // 打印获取到的信息
        av_dump_format(pFormatCtx, 0, filename.c_str(), 0);

        //找出视频流
        int videoindex = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (videoindex < 0) {
            av_strerror(videoindex, errStr, sizeof(errStr));
            std::cout << "find best video stream failed, " << errStr << std::endl;
        }

        //找出音频流
        int audioindex = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        if (audioindex < 0) {
            av_strerror(audioindex, errStr, sizeof(errStr));
            std::cout << "find best audio stream failed, " << errStr << std::endl;
        }

        // 如果视频流和音频流都不存在，则退出
        if (videoindex < 0 && audioindex < 0) {
            break;
        }

        AVPacket *pkt = av_packet_alloc();

        for (int i=0; i<10; i++) {
            // 读取一帧
            ret = av_read_frame(pFormatCtx, pkt);
            if (ret >= 0) {
                // 根据pkt的stream_index成员，分离音频/视频
                if (pkt->stream_index == videoindex) {
                    std::cout << "read video pkt, size: " << pkt->size << std::endl;
                } else if (pkt->stream_index == audioindex) {
                    std::cout << "read audio pkt, size: " << pkt->size << std::endl;
                }
            }
        }

        av_packet_free(&pkt);
    } while (0);

    // 释放资源
    if (pFormatCtx) {
        avformat_close_input(&pFormatCtx);
    }
    return 0;
}