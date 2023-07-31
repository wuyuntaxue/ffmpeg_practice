#ifdef __cplusplus
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_id.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
}
#endif

#include <cstdio>
#include <iostream>
#include <string>


char        errStr[10240] = {0};
std::string filename      = "out.yuv";
std::string outfilename   = "test.h264";
int         width_        = 1920;
int         height_       = 1080;

AVPixelFormat sw_format = AV_PIX_FMT_NV12;
AVPixelFormat hw_format = AV_PIX_FMT_QSV;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "param: infile outfile" << std::endl;
        return -1;
    }
    filename    = argv[1];
    outfilename = argv[2];


    av_log_set_level(AV_LOG_DEBUG);
    int ret = 0;

    // 1. hw device create
    AVBufferRef *device_ctx_ref;
    ret = av_hwdevice_ctx_create(&device_ctx_ref, AV_HWDEVICE_TYPE_QSV, NULL, NULL, 0);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "hwdevice ctx create failed, " << errStr << std::endl;
        return -1;
    }

    // 2. hw frame ctx alloc and init
    AVBufferRef *hw_frames_ctx_ref;
    hw_frames_ctx_ref = av_hwframe_ctx_alloc(device_ctx_ref);
    if (hw_frames_ctx_ref == nullptr) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "av_hwframe_ctx_alloc failed, " << errStr << std::endl;
        return -1;
    }

    AVHWFramesContext *hw_frames_ctx = (AVHWFramesContext *)hw_frames_ctx_ref->data;
    hw_frames_ctx->format            = hw_format;
    hw_frames_ctx->sw_format         = sw_format;
    hw_frames_ctx->width             = width_;
    hw_frames_ctx->height            = height_;
    hw_frames_ctx->initial_pool_size = 1;

    ret = av_hwframe_ctx_init(hw_frames_ctx_ref);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "av_hwframe_ctx_init failed, " << errStr << std::endl;
        return -1;
    }

    // 3
    const AVCodec *encodec_ = avcodec_find_encoder_by_name("mjpeg_qsv");
    if (encodec_ == nullptr) {
        std::cout << "codec not find" << std::endl;
        return -1;
    }

    // 4
    AVCodecContext *encodec_ctx_ = avcodec_alloc_context3(encodec_);
    if (encodec_ctx_ == nullptr) {
        std::cout << "codec ctx alloc failed" << std::endl;
        return -1;
    }

    encodec_ctx_->width     = width_;
    encodec_ctx_->height    = height_;
    encodec_ctx_->time_base = AVRational{1, 25};
    encodec_ctx_->framerate = AVRational{25, 1};
    encodec_ctx_->gop_size  = 10;
    // encodec_ctx_->pkt_timebase  = AVRational{1, 90000};
    encodec_ctx_->pix_fmt       = hw_format;
    encodec_ctx_->hw_frames_ctx = av_buffer_ref(hw_frames_ctx_ref); // !

    // 5
    ret = avcodec_open2(encodec_ctx_, encodec_, NULL);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "avcodec_open2 failed, " << errStr << std::endl;
        return -1;
    }

    std::cout << "init done" << std::endl;


    //////////////////////////////////////////////////////////////////////////////////

    /// read yuv file

    FILE *pFile = fopen(filename.c_str(), "rb");
    if (pFile == nullptr) {
        std::cout << "open file[" << filename << "] failed" << std::endl;
        return -1;
    }

    FILE *pOutFile = fopen(outfilename.c_str(), "wb");
    if (pOutFile == nullptr) {
        std::cout << "open file[" << outfilename << "] failed" << std::endl;
        return -1;
    }

    std::cout << "start read file: " << filename << ", width: " << encodec_ctx_->width
              << ", height: " << encodec_ctx_->height;

    // user memery frame
    AVFrame *frame = av_frame_alloc();
    frame->width   = encodec_ctx_->width;
    frame->height  = encodec_ctx_->height;
    frame->format  = sw_format;
    frame->pts     = 0;
    ret            = av_frame_get_buffer(frame, 0);
    // ret = av_image_alloc(frame->data, frame->linesize, encodec_ctx_->width, encodec_ctx_->height,
    //                      sw_format, 16);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "av_frame_get_buffer failed, " << errStr << std::endl;
        return -1;
    }

    // hw memery frame
    AVFrame *frame_hw_mem = av_frame_alloc();
    frame_hw_mem->width   = encodec_ctx_->width;
    frame_hw_mem->height  = encodec_ctx_->height;
    frame_hw_mem->format  = hw_format;
    ret                   = av_hwframe_get_buffer(hw_frames_ctx_ref, frame_hw_mem, 0);
    if (ret < 0) {
        av_strerror(ret, errStr, sizeof(errStr));
        std::cout << "av_hwframe_get_buffer failed, " << errStr << std::endl;
        return -1;
    }

    char uBuffer[frame->width * frame->height / 4];
    char vBuffer[frame->width * frame->height / 4];


    while (!feof(pFile)) {

        memset(uBuffer, 0, sizeof(uBuffer));
        memset(vBuffer, 0, sizeof(vBuffer));

        fread(frame->data[0], 1, frame->width * frame->height, pFile); // y
        fread(uBuffer, 1, frame->width * frame->height / 4, pFile);    // u
        fread(vBuffer, 1, frame->width * frame->height / 4, pFile);    // v
        // 这里读取YUV420P的文件，转换成NV12格式
        for (int i = 0; i < frame->width * frame->height / 4; i++) {
            frame->data[1][i * 2]     = uBuffer[i];
            frame->data[1][i * 2 + 1] = vBuffer[i];
        }

        // NV12
        // fread(frame->data[1], 1, frame->width * frame->height / 2, pFile); // uv

        // YUV420P
        // fread(frame->data[1], 1, frame->width * frame->height / 4, pFile); // u
        // fread(frame->data[2], 1, frame->width * frame->height / 4, pFile); // v

        frame->pts += 1000 / (encodec_ctx_->time_base.den / encodec_ctx_->time_base.num);

        // transfer user to hw
        ret = av_hwframe_transfer_data(frame_hw_mem, frame, 0);
        if (ret < 0) {
            av_strerror(ret, errStr, sizeof(errStr));
            std::cout << "av_hwframe_transfer_data failed, " << errStr << std::endl;
            return -1;
        }
        frame_hw_mem->pts = frame->pts;

        // send frame
        {
            ret = avcodec_send_frame(encodec_ctx_, frame_hw_mem);
            if (ret < 0) {
                av_strerror(ret, errStr, sizeof(errStr));
                std::cout << "Error sending a frame for encoding, " << errStr << std::endl;
                return -1;
            }

            AVPacket *pkt = av_packet_alloc();

            while (ret == 0) {

                ret = avcodec_receive_packet(encodec_ctx_, pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error encoding video frame\n");
                    return -1;
                }

                if (ret == 0) {
                    fwrite(pkt->data, 1, pkt->size, pOutFile);
                }
            }

            av_packet_free(&pkt);
        }
    }

    av_frame_free(&frame);
    fclose(pFile);
    fclose(pOutFile);

    return 0;
}