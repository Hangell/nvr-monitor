#include "nvr/decoder.h"
#include <errno.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

int nvr_decoder_open(NvrDecoder *d, const AVCodecParameters *parameters) {
    if (!d || !parameters) return -1;
    memset(d, 0, sizeof(*d));
    const AVCodec *codec = avcodec_find_decoder(parameters->codec_id);
    if (!codec) return AVERROR_DECODER_NOT_FOUND;
    d->codec = avcodec_alloc_context3(codec);
    if (!d->codec) return AVERROR(ENOMEM);
    int result = avcodec_parameters_to_context(d->codec, parameters);
    if (result >= 0) result = avcodec_open2(d->codec, codec, NULL);
    if (result < 0) { nvr_decoder_close(d); return result; }
    d->decoded = av_frame_alloc();
    d->rgba = av_frame_alloc();
    if (!d->decoded || !d->rgba) { nvr_decoder_close(d); return AVERROR(ENOMEM); }
    return 0;
}

int nvr_decoder_send(NvrDecoder *d, const AVPacket *packet) {
    return d && d->codec ? avcodec_send_packet(d->codec, packet) : -1;
}

int nvr_decoder_receive_rgba(NvrDecoder *d, uint8_t **pixels, int *width, int *height, int *pitch, int convert) {
    int result = avcodec_receive_frame(d->codec, d->decoded);
    if (result < 0) return result;
    /* Drain reference frames even when no display needs a BGRA copy. */
    if (!convert) return 0;
    if (d->width != d->decoded->width || d->height != d->decoded->height) {
        sws_freeContext(d->sws); d->sws = NULL;
        av_freep(&d->rgba_buffer);
        d->width = d->decoded->width; d->height = d->decoded->height;
        d->sws = sws_getContext(d->width, d->height, d->decoded->format,
                                d->width, d->height, AV_PIX_FMT_BGRA,
                                SWS_FAST_BILINEAR, NULL, NULL, NULL);
        int bytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, d->width, d->height, 1);
        d->rgba_buffer = av_malloc((size_t)bytes);
        if (!d->sws || !d->rgba_buffer) return AVERROR(ENOMEM);
        av_image_fill_arrays(d->rgba->data, d->rgba->linesize, d->rgba_buffer,
                             AV_PIX_FMT_BGRA, d->width, d->height, 1);
    }
    sws_scale(d->sws, (const uint8_t *const *)d->decoded->data, d->decoded->linesize,
              0, d->height, d->rgba->data, d->rgba->linesize);
    *pixels = d->rgba->data[0]; *width = d->width; *height = d->height; *pitch = d->rgba->linesize[0];
    return 0;
}

void nvr_decoder_close(NvrDecoder *d) {
    if (!d) return;
    sws_freeContext(d->sws);
    av_freep(&d->rgba_buffer);
    av_frame_free(&d->rgba);
    av_frame_free(&d->decoded);
    avcodec_free_context(&d->codec);
    memset(d, 0, sizeof(*d));
}
