#include "h264encoder.h"

int main() {
    av_log_set_level(AV_LOG_TRACE);
    FFMPEGEncoder encoder;

    encoder.init_encoder();
    encoder.working("../../movie/out2.yuv", "hello.h264");

    encoder.deinit_encoder();

    return 0;
}