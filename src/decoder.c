#include "nvr/decoder.h"
#include "nvr/logger.h"
#include <errno.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

static enum AVPixelFormat hardware_format(AVCodecContext *context, const enum AVPixelFormat *formats) {
    NvrDecoder *d = context->opaque;
    for (; *formats != AV_PIX_FMT_NONE; ++formats)
        if (*formats == d->hw_format) return *formats;
    return AV_PIX_FMT_NONE;
}

static int open_context(NvrDecoder *d, const AVCodec *codec, const AVCodecHWConfig *hw) {
    d->codec = avcodec_alloc_context3(codec);
    if (!d->codec) return AVERROR(ENOMEM);
    int result = avcodec_parameters_to_context(d->codec, d->parameters);
    if (result >= 0 && hw) {
        result = av_hwdevice_ctx_create(&d->codec->hw_device_ctx, hw->device_type, NULL, NULL, 0);
        d->hw_format = hw->pix_fmt;
        d->codec->opaque = d;
        d->codec->get_format = hardware_format;
    }
    if (result >= 0) result = avcodec_open2(d->codec, codec, NULL);
    if (result < 0) avcodec_free_context(&d->codec);
    return result;
}

static int fallback_cpu(NvrDecoder *d, int error, const char *stage) {
    char reason[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(error, reason, sizeof(reason));
    nvr_log(NVR_LOG_WARN, "Decodificação GPU falhou em %s (%s); usando CPU até a próxima conexão", stage, reason);
    av_frame_unref(d->decoded);
    av_frame_unref(d->software);
    avcodec_free_context(&d->codec);
    d->hw_format = AV_PIX_FMT_NONE;
    d->wait_keyframe = 1;
    return open_context(d, avcodec_find_decoder(d->parameters->codec_id), NULL);
}

int nvr_decoder_open(NvrDecoder *d, const AVCodecParameters *parameters) {
    if (!d || !parameters) return AVERROR(EINVAL);
    memset(d, 0, sizeof(*d));
    d->hw_format = AV_PIX_FMT_NONE;
    const AVCodec *codec = avcodec_find_decoder(parameters->codec_id);
    if (!codec) return AVERROR_DECODER_NOT_FOUND;
    d->parameters = avcodec_parameters_alloc();
    d->decoded = av_frame_alloc();
    d->software = av_frame_alloc();
    d->rgba = av_frame_alloc();
    if (!d->parameters || !d->decoded || !d->software || !d->rgba) {
        nvr_decoder_close(d); return AVERROR(ENOMEM);
    }
    int result = avcodec_parameters_copy(d->parameters, parameters);
    if (result < 0) { nvr_decoder_close(d); return result; }
    /* Prefer the native platform backend, then NVIDIA, when supported by the codec. */
    static const enum AVHWDeviceType devices[] = {
#ifdef _WIN32
        AV_HWDEVICE_TYPE_D3D11VA, AV_HWDEVICE_TYPE_DXVA2,
#elif defined(__APPLE__)
        AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
#else
        AV_HWDEVICE_TYPE_VAAPI,
#endif
        AV_HWDEVICE_TYPE_CUDA
    };
    for (size_t device = 0; device < sizeof(devices) / sizeof(devices[0]); ++device) {
        for (int i = 0; ; ++i) {
            const AVCodecHWConfig *hw = avcodec_get_hw_config(codec, i);
            if (!hw) break;
            if (hw->device_type != devices[device] ||
                !(hw->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) continue;
            if (open_context(d, codec, hw) >= 0) {
                /* RTSP may start mid-GOP; hardware needs a complete reference chain. */
                d->wait_keyframe = 1;
                nvr_log(NVR_LOG_INFO, "Decodificador %s: GPU %s selecionada", codec->name,
                        av_hwdevice_get_type_name(hw->device_type));
                return 0;
            }
        }
    }
    d->hw_format = AV_PIX_FMT_NONE;
    result = open_context(d, codec, NULL);
    if (result < 0) { nvr_decoder_close(d); return result; }
    nvr_log(NVR_LOG_INFO, "Decodificador %s: CPU (GPU compatível indisponível)", codec->name);
    return 0;
}

int nvr_decoder_send(NvrDecoder *d, const AVPacket *packet) {
    if (!d || !d->codec) return AVERROR(EINVAL);
    if (d->wait_keyframe && packet) {
        if (!(packet->flags & AV_PKT_FLAG_KEY)) return 0;
        d->wait_keyframe = 0;
    }
    int result = avcodec_send_packet(d->codec, packet);
    if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF &&
        d->hw_format != AV_PIX_FMT_NONE) {
        int fallback = fallback_cpu(d, result, "envio do pacote");
        if (fallback < 0) return fallback;
        return nvr_decoder_send(d, packet);
    }
    return result;
}

int nvr_decoder_receive_rgba(NvrDecoder *d, uint8_t **pixels, int *width, int *height, int *pitch, int convert) {
    if (!d || !d->codec) return AVERROR(EINVAL);
    int result = avcodec_receive_frame(d->codec, d->decoded);
    if (result < 0) {
        if (result != AVERROR(EAGAIN) && result != AVERROR_EOF && d->hw_format != AV_PIX_FMT_NONE) {
            int fallback = fallback_cpu(d, result, "recepção do quadro");
            if (fallback < 0) return fallback;
            return AVERROR(EAGAIN);
        }
        return result;
    }
    /* Drain reference frames without downloading GPU frames when hidden. */
    if (!convert) { av_frame_unref(d->decoded); return 0; }
    AVFrame *frame = d->decoded;
    if (frame->hw_frames_ctx) {
        av_frame_unref(d->software);
        result = av_hwframe_transfer_data(d->software, frame, 0);
        if (result < 0) {
            int fallback = fallback_cpu(d, result, "transferência do quadro");
            return fallback < 0 ? fallback : AVERROR(EAGAIN);
        }
        frame = d->software;
    }
    d->sws = sws_getCachedContext(d->sws, frame->width, frame->height, frame->format,
                                 frame->width, frame->height, AV_PIX_FMT_BGRA,
                                 SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (!d->sws) return AVERROR(ENOMEM);
    if (!d->rgba_buffer || d->width != frame->width || d->height != frame->height) {
        av_freep(&d->rgba_buffer);
        int bytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, frame->width, frame->height, 1);
        if (bytes < 0) return bytes;
        d->rgba_buffer = av_malloc((size_t)bytes);
        if (!d->rgba_buffer) return AVERROR(ENOMEM);
        d->width = frame->width; d->height = frame->height;
        result = av_image_fill_arrays(d->rgba->data, d->rgba->linesize, d->rgba_buffer,
                                     AV_PIX_FMT_BGRA, d->width, d->height, 1);
        if (result < 0) return result;
    }
    result = sws_scale(d->sws, (const uint8_t *const *)frame->data, frame->linesize,
                      0, d->height, d->rgba->data, d->rgba->linesize);
    av_frame_unref(d->decoded);
    av_frame_unref(d->software);
    if (result < 0) return result;
    *pixels = d->rgba->data[0]; *width = d->width; *height = d->height; *pitch = d->rgba->linesize[0];
    return 0;
}

void nvr_decoder_close(NvrDecoder *d) {
    if (!d) return;
    sws_freeContext(d->sws);
    av_freep(&d->rgba_buffer);
    av_frame_free(&d->rgba);
    av_frame_free(&d->software);
    av_frame_free(&d->decoded);
    avcodec_free_context(&d->codec);
    avcodec_parameters_free(&d->parameters);
    memset(d, 0, sizeof(*d));
}
