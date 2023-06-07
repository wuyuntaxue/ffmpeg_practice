/**
 * @brief 从多媒体文件中获取、分离音频流，并将音频数据单独保存在另一文件中
 * 程序假设多媒体文件中的音频格式是aac、采样率是48000、双声道（怎么在程序里获取还没学到）
 */

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
}

#define ADTS_HEAD_LEN 7

// 采样率列表
static unsigned const samplingFrequencyTable[16] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                                    16000, 12000, 11025, 8000,  7350,  0,     0,     0};

/**
 * @brief 填ADTS的帧头信息
 * 
 * @param szAdtsHeader 预留的帧头地址，7字节
 * @param dataLen 这一帧的数据长度
 */
void adts_header(char *szAdtsHeader, int dataLen) {

    int audio_object_type = 2;
    // 通过av_dump_format显示音频信息或者ffplay获取多媒体文件的音频流编码acc（LC），对应表格中Object Type ID
    // -- 2 int sampling_frequency_index = 4;      //音频信息中采样率为44100 Hz 对应采样率索引0x4
    int sampling_frequency_index = 3;
    int channel_config           = 2;
    // 音频信息中音频通道为双通道2

    int adtsLen = dataLen + 7;
    // 采用头长度为7字节，所以protection_absent=1   =0时为9字节，表示含有CRC校验码

    szAdtsHeader[0] = 0xff;
    szAdtsHeader[1] = 0xf0;
    // syncword ：总是0xFFF, 代表一个ADTS帧的开始, 用于同步. 高8bits

    szAdtsHeader[1] |= (0 << 3); // MPEG Version:0 : MPEG-4(mp4a),1 : MPEG-2  1bit
    szAdtsHeader[1] |= (0 << 1); // Layer:0                                   2bits
    szAdtsHeader[1] |= 1;        // protection absent:1  没有CRC校验            1bit

    szAdtsHeader[2] = (audio_object_type - 1) << 6;
    // profile=(audio_object_type - 1) 表示使用哪个级别的AAC  2bits
    szAdtsHeader[2] |= (sampling_frequency_index & 0x0f) << 2;
    // sampling frequency index:sampling_frequency_index  4bits
    szAdtsHeader[2] |= (0 << 1);
    // private bit:0                                      1bit
    szAdtsHeader[2] |= (channel_config & 0x04) >> 2;
    // channel configuration:channel_config 高1bit

    szAdtsHeader[3] = (channel_config & 0x03) << 6;
    // channel configuration:channel_config      低2bits
    szAdtsHeader[3] |= (0 << 5);
    // original：0                               1bit
    szAdtsHeader[3] |= (0 << 4);
    // home：0                                   1bit ----------------固定头完结，开始可变头
    szAdtsHeader[3] |= (0 << 3);
    // copyright id bit：0                       1bit
    szAdtsHeader[3] |= (0 << 2);
    // copyright id start：0                     1bit
    szAdtsHeader[3] |= ((adtsLen & 0x1800) >> 11);
    // frame length：value                       高2bits  000|1 1000|0000 0000

    szAdtsHeader[4] = (uint8_t)((adtsLen & 0x7f8) >> 3);
    // frame length:value    中间8bits             0000  0111 1111 1000

    szAdtsHeader[5] = (uint8_t)((adtsLen & 0x7) << 5);
    // frame length:value    低 3bits              0000  0000 0000 0111
    // number_of_raw_data_blocks_in_frame：表示ADTS帧中有number_of_raw_data_blocks_in_frame +
    // 1个AAC原始帧。所以说number_of_raw_data_blocks_in_frame == 0
    // 表示说ADTS帧中有一个AAC数据块。(一个AAC原始帧包含一段时间内1024个采样及相关数据)
    szAdtsHeader[5] |= 0x1f;
    // buffer fullness:0x7ff 高5bits   0x7FF 说明是码率可变的码流 ---> 111 1111 1111
    // 00----> 1 1111 1111 1100--->0x1f与0xfc

    szAdtsHeader[6] = 0xfc;
}

int main(int argc, char *argv[]) {

    av_log_set_level(AV_LOG_INFO);
    if (argc < 3) {
        av_log(NULL, AV_LOG_ERROR, "miss param\n");
        return -1;
    }

    const char *srcFile = argv[1];
    const char *dstFile = argv[2];

    /// 打开多媒体文件
    AVFormatContext *fmt_ctx = NULL;
    int              ret     = avformat_open_input(&fmt_ctx, srcFile, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "avformat open failed\n");
        return -1;
    }

    /// 获取最合适的流，指定类型为AUDIO
    int streamidx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (streamidx == AVERROR_STREAM_NOT_FOUND || streamidx == AVERROR_DECODER_NOT_FOUND) {
        av_log(NULL, AV_LOG_ERROR, "find best stream failed\n");
        return -1;
    }

    /// 打印媒体信息
    av_dump_format(fmt_ctx, streamidx, srcFile, 0);


    AVPacket pkt;
    char     adtsHeaderBuf[ADTS_HEAD_LEN] = {0};

    /// 打开要写入的文件
    FILE *dtsFd = fopen(dstFile, "wb");
    if (dtsFd == nullptr) {
        av_log(NULL, AV_LOG_ERROR, "dts file open failed\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    /// 循环获取帧
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == streamidx) {
            /// 按照ADTS的格式写aac文件

            // 先产生帧头、写入文件
            adts_header(adtsHeaderBuf, pkt.size);
            int size = fwrite(adtsHeaderBuf, ADTS_HEAD_LEN, 1, dtsFd);
            if (size != 1) {
                av_log(NULL, AV_LOG_ERROR, "write adts header failed\n");
                break;
            }

            // 再写入数据
            size = fwrite(pkt.data, pkt.size, 1, dtsFd);
            if (size != 1) {
                av_log(NULL, AV_LOG_ERROR, "write pkt.data failed\n");
                break;
            }

        } else {
            // av_log(NULL, AV_LOG_WARNING, "other stream idx\n");
        }

        // 使用之后，需要减去引用计数
        av_packet_unref(&pkt);
    }
    av_packet_unref(&pkt);

    av_log(NULL, AV_LOG_INFO, "test done\n");

    fclose(dtsFd);
    avformat_close_input(&fmt_ctx);
    return 0;
}