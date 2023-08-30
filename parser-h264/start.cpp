#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libswscale/swscale.h>
}

#include "sw-h264-transcoder.h"
#include <memory>

int main(int argc, char *argv[]) {

    if (argc < 2) {
        std::cout << argv[0] << " intputfile outputfile" << std::endl;
        return -1;
    }

    SWTranscoder transcoder_;
    int          ret = transcoder_.open(AV_CODEC_ID_H264, AV_CODEC_ID_PNG);

    if (ret == 0) {
        transcoder_.transcode(argv[1], argv[2]);
    }

    transcoder_.close();

    return 0;
}