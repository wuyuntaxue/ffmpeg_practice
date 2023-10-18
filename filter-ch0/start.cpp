/*
 * Copyright (c) 2010 Nicolas George
 * Copyright (c) 2011 Stefano Sabatini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * API example for decoding and filtering
 * @example decode_filter_video.c
 */

#define _XOPEN_SOURCE 600 /* for usleep */

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
}

#include <functional>
#include <iostream>

// const char *filter_descr = "scale=200:300,transpose=cclock";
const char *filter_descr = "scale=960:720";
/* other way:
   scale=78:24 [scl]; [scl] transpose=cclock // assumes "[in]" and "[out]" to be input output pads
   respectively
 */

static AVFormatContext *fmt_ctx;
static AVCodecContext  *dec_ctx;
AVFilterContext        *buffersink_ctx;
AVFilterContext        *buffersrc_ctx;
AVFilterGraph          *filter_graph;
static int              video_stream_index = -1;
static int64_t          last_pts           = AV_NOPTS_VALUE;

static char  errStr[1280] = {0};
static char *av_get_err(int errnum) {
    av_strerror(errnum, errStr, 1280);
    return errStr;
}

namespace Encodec {
    // encoder
    const AVCodec  *pEncodec_       = nullptr;
    AVCodecContext *pEncodec_ctx_   = nullptr;
    bool            encoder_opened_ = false;

    using EncodeCallback = std::function<void(unsigned int frameid, AVPacket *outpkt)>;

    int open_encoder(AVCodecID codecID, AVPixelFormat inPixel, int width, int height) {
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

    int close_encoder() {
        encoder_opened_ = false;
        if (pEncodec_ctx_) {
            avcodec_close(pEncodec_ctx_);
            avcodec_free_context(&pEncodec_ctx_);
        }
        return 0;
    }

    int do_encode(unsigned int frameid, AVFrame *inframe, EncodeCallback callback) {
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
} // namespace Encodec

static int open_input_file(const char *filename) {
    const AVCodec *dec;
    int            ret;

    if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    /* select the video stream */
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        return ret;
    }
    video_stream_index = ret;

    /* create decoding context */
    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx)
        return AVERROR(ENOMEM);
    avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);

    /* init the video decoder */
    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
        return ret;
    }

    return 0;
}

static int init_filters(const char *filters_descr) {
    char               args[512];
    int                ret        = 0;
    const AVFilter    *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter    *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut     *outputs    = avfilter_inout_alloc();
    AVFilterInOut     *inputs     = avfilter_inout_alloc();
    AVRational         time_base  = fmt_ctx->streams[video_stream_index]->time_base;
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};

    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt, time_base.num, time_base.den,
             dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr, &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static void display_frame(AVFrame *frame, AVRational time_base) {
    {
        static int64_t countPts=0;
        if (!Encodec::encoder_opened_) {
            Encodec::open_encoder(AV_CODEC_ID_MJPEG, (AVPixelFormat)frame->format, frame->width,
                                  frame->height);
        }

        if (Encodec::encoder_opened_) {
            // std::cout << "pts: " << frame->pts << std::endl;
            frame->pts = ++countPts;

            Encodec::do_encode(1, frame, [](int frameid, AVPacket *outpkt) {
                FILE *pOutFile = fopen("hello.mjpeg", "a+");
                if (pOutFile == nullptr) {
                    std::cout << "out file open failed" << std::endl;
                    return;
                }
                fwrite(outpkt->data, 1, outpkt->size, pOutFile);
                fclose(pOutFile);
            });
        }
    }
    // int      x, y;
    // uint8_t *p0, *p;
    // int64_t  delay;

    // if (frame->pts != AV_NOPTS_VALUE) {
    //     if (last_pts != AV_NOPTS_VALUE) {
    //         /* sleep roughly the right amount of time;
    //          * usleep is in microseconds, just like AV_TIME_BASE. */
    //         delay = av_rescale_q(frame->pts - last_pts, time_base, AV_TIME_BASE_Q);
    //         if (delay > 0 && delay < 1000000)
    //             usleep(delay);
    //     }
    //     last_pts = frame->pts;
    // }

    // /* Trivial ASCII grayscale display. */
    // p0 = frame->data[0];
    // puts("\033c");
    // for (y = 0; y < frame->height; y++) {
    //     p = p0;
    //     for (x = 0; x < frame->width; x++)
    //         putchar(" .-+#"[*(p++) / 52]);
    //     putchar('\n');
    //     p0 += frame->linesize[0];
    // }
    // fflush(stdout);
}

int main(int argc, char **argv) {
    int       ret;
    AVPacket *packet;
    AVFrame  *frame;
    AVFrame  *filt_frame;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        exit(1);
    }

    av_log_set_level(AV_LOG_DEBUG);

    frame      = av_frame_alloc();
    filt_frame = av_frame_alloc();
    packet     = av_packet_alloc();
    if (!frame || !filt_frame || !packet) {
        fprintf(stderr, "Could not allocate frame or packet\n");
        exit(1);
    }

    if ((ret = open_input_file(argv[1])) < 0)
        goto end;
    if ((ret = init_filters(filter_descr)) < 0)
        goto end;

    /* read all packets */
    while (1) {
        if ((ret = av_read_frame(fmt_ctx, packet)) < 0)
            break;

        if (packet->stream_index == video_stream_index) {
            ret = avcodec_send_packet(dec_ctx, packet);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(dec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
                    goto end;
                }

                frame->pts = frame->best_effort_timestamp;

                /* push the decoded frame into the filtergraph */
                if (av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                    break;
                }

                /* pull filtered frames from the filtergraph */
                while (1) {
                    ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    if (ret < 0)
                        goto end;
                    display_frame(filt_frame, buffersink_ctx->inputs[0]->time_base);
                    av_frame_unref(filt_frame);
                }
                av_frame_unref(frame);
            }
        }
        av_packet_unref(packet);
    }

end:
    avfilter_graph_free(&filter_graph);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    av_frame_free(&filt_frame);
    av_packet_free(&packet);

    if (Encodec::encoder_opened_) {
        Encodec::close_encoder();
    }

    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_get_err(ret));
        exit(1);
    }

    exit(0);
}
