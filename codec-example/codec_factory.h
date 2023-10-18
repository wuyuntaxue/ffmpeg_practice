#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

#include "decoder.h"
#include "encoder.h"
#include "qsv_decoder.h"
#include "qsv_encoder.h"
#include "sw_decoder.h"
#include "sw_encoder.h"
#include "vaapi_decoder.h"
#include "vaapi_encoder.h"

#include <functional>
#include <string>

namespace Codec {

    enum NCodecMode { SW_CODEC = 0, VAAPI_CODEC, QSV_CODEC, CODEC_MODE_INVAILD };

    static BasicEncoder::Ptr CreateEncoder(NCodecMode mode, int chn) {
        switch (mode) {
        case SW_CODEC: {
            return std::make_shared<SwEncoder>(chn);
        } break;
        case VAAPI_CODEC: {
            return std::make_shared<VAAPIEncoder>(chn);
        } break;
        case QSV_CODEC: {
            return std::make_shared<QSVEncoder>(chn);
        } break;
        default:
            break;
        }
        return nullptr;
    }


    static BasicDecoder::Ptr CreateDecoder(NCodecMode mode, int chn) {
        switch (mode) {
        case SW_CODEC: {
            return std::make_shared<SwDecoder>(chn);
        } break;
        case VAAPI_CODEC: {
            return std::make_shared<VAAPIDecoder>(chn);
        } break;
        case QSV_CODEC: {
            return std::make_shared<QSVDecoder>(chn);
        } break;
        default:
            break;
        }
        return nullptr;
    }
} // namespace Codec