#ifndef NVR_DECODER_H
#define NVR_DECODER_H

#include <stdint.h>
typedef struct AVCodecContext AVCodecContext;
typedef struct AVFrame AVFrame;
typedef struct AVPacket AVPacket;
typedef struct SwsContext SwsContext;
typedef struct AVCodecParameters AVCodecParameters;

typedef struct {
    AVCodecContext *codec;
    AVFrame *decoded;
    AVFrame *rgba;
    SwsContext *sws;
    uint8_t *rgba_buffer;
    int width;
    int height;
} NvrDecoder;

int nvr_decoder_open(NvrDecoder *decoder, const AVCodecParameters *parameters);
int nvr_decoder_send(NvrDecoder *decoder, const AVPacket *packet);
int nvr_decoder_receive_rgba(NvrDecoder *decoder, uint8_t **pixels,
                             int *width, int *height, int *pitch);
void nvr_decoder_close(NvrDecoder *decoder);

#endif
