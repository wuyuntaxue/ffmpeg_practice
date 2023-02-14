//#include <stdio.h>

/// ffmpeg的库都是C语言，调用时需要指示编译器按照C语言格式链接
extern "C"{
    #include <libavutil/log.h>
    #include <libavformat/avformat.h>
}

int main(int argc, char* argv[])
{
    int ret;
    AVFormatContext* fmt_ctx = NULL;
    AVStream* stm = NULL;
    AVPacket* pkt = NULL;

    av_log_set_level(AV_LOG_INFO);
    //日志
    av_log( NULL, AV_LOG_WARNING, "...Hello world:%s %s\n", argv[0], argv[1]);

    //打开文件或URL，解析媒体的头信息，创建AVFormatContext并填充一些关键信息
    ret = avformat_open_input( &fmt_ctx, argv[1], NULL, NULL);
    if(ret<0){
        av_log(NULL, AV_LOG_INFO,"Can`t open file: %d\n", (ret));
        return -1;
    }
    
    //获取流详细信息
    ret = avformat_find_stream_info(fmt_ctx, 0); 
    if(ret<0){
        av_log(NULL,AV_LOG_WARNING,"Can`t get stream information!just show aac, not show aac(LC)!\n");
    }

    //打印相关信息
    av_dump_format(fmt_ctx, 0, argv[1], 0); //第一个0是流的索引值,，第二个表示输入/输出流，由于是输入文件，所以为0
    //关闭上下文
    avformat_close_input(&fmt_ctx);
    return 0;
}

// g++ -o start start.cpp -lavutil -lavformat
