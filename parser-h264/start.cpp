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

#include "h264decoder.h"

int main() {
    FFMPEGDecoder decoder("test_cash.h264", "out.yuv");

    decoder.init_decoder();

    decoder.decode();

    decoder.deinit_decoder();
    return 0;
}