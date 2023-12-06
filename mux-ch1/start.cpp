extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <iostream>
#include <string>

static char errStr[1024] = {0};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "should: " << argv[0] << " inputfile outputfile" << std::endl;
        return -1;
    }

    std::string infile(argv[1]);
    std::string outfile(argv[2]);

    av_log_set_level(AV_LOG_DEBUG);

    /********************/
    AVFormatContext *pFormatCtx_ = nullptr;
    int              ret         = avformat_open_input(&pFormatCtx_, infile.c_str(), nullptr, nullptr);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "format open failed, " << errStr << std::endl;
        return -1;
    }

    avformat_find_stream_info(pFormatCtx_, NULL);

    int videoindex = av_find_best_stream(pFormatCtx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoindex < 0) {
        std::cout << "can't find video stream" << std::endl;
    }
    /********************/

    AVFormatContext *pFormatOutCtx_ = nullptr;
    // avformat_write_header()
    avformat_alloc_output_context2(&pFormatOutCtx_, nullptr, "mp4", outfile.c_str());
    if (pFormatOutCtx_ == nullptr) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "avformat_alloc_output_context2 failed, " << errStr << std::endl;
        return -1;
    }

    AVStream *outStream_ = avformat_new_stream(pFormatOutCtx_, nullptr);
    if (outStream_ == nullptr) {
        std::cout << "new stream failed" << std::endl;
        return -1;
    }

    ret = avcodec_parameters_copy(outStream_->codecpar, pFormatCtx_->streams[videoindex]->codecpar);
    // avcodec_parameters_from_context(
    // avcodec_parameters_from_context(outStream_->codecpar, );
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "parameter copy failed, " << errStr << std::endl;
        return -1;
    }

    ret = avio_open(&pFormatOutCtx_->pb, outfile.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "avio_open failed, " << errStr << std::endl;
        return -1;
    }

    ret = avformat_write_header(pFormatOutCtx_, nullptr);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "avformat_write_header failed, " << errStr << std::endl;
        return -1;
    }

    uint64_t pktCount = 0;

    while (ret >= 0) {
        AVPacket *readPkt = av_packet_alloc();
        ret               = av_read_frame(pFormatCtx_, readPkt);
        if (ret == AVERROR_EOF) {
            std::cout << "read eof" << std::endl;
            break;
        } else if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            std::cout << "read frame failed, " << errStr << std::endl;
            break;
        }
        if (readPkt->stream_index == videoindex) {

            // 输入为mp4或这rtsp流时正常，输入为H264裸文件时，时间戳总是不对
            readPkt->pts = av_rescale_q_rnd(readPkt->pts, pFormatCtx_->streams[videoindex]->time_base,
                                            pFormatOutCtx_->streams[readPkt->stream_index]->time_base,
                                            AV_ROUND_PASS_MINMAX);

            readPkt->dts = readPkt->pts;

            readPkt->duration = av_rescale_q(readPkt->duration, pFormatCtx_->streams[videoindex]->time_base,
                                             pFormatOutCtx_->streams[readPkt->stream_index]->time_base);

            readPkt->pos = -1;

            // av_write_frame(pFormatOutCtx_, readPkt);
            av_interleaved_write_frame(pFormatOutCtx_, readPkt);

            pktCount++;
            if (pktCount > 2000) {
                break;
            }
        }
        av_packet_free(&readPkt);
    }
    av_write_trailer(pFormatOutCtx_);

    avio_closep(&pFormatOutCtx_->pb);
    avformat_free_context(pFormatOutCtx_);

    avformat_close_input(&pFormatCtx_);

    std::cout << "done" << std::endl;

    return 0;
}