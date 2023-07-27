#include "h264encoder.h"

int main() {
    FFMPEGEncoder encoder;

    encoder.init_encoder();
    encoder.working("../../movie/out.yuv", "hello.mjpeg");

    encoder.deinit_encoder();

    return 0;
}