#include "nvr/decoder.h"
#include <assert.h>
#include <errno.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>

static int device_attempts;
int __wrap_av_hwdevice_ctx_create(AVBufferRef **device, enum AVHWDeviceType type,
                                 const char *name, AVDictionary *options, int flags) {
    (void)device; (void)type; (void)name; (void)options; (void)flags;
    ++device_attempts;
    return AVERROR(ENOSYS);
}

static void check_frame(NvrDecoder *decoder, int convert) {
    uint8_t *pixels = NULL;
    int width = 0, height = 0, pitch = 0;
    assert(nvr_decoder_receive_rgba(decoder, &pixels, &width, &height, &pitch, convert) == 0);
    if (convert) {
        assert(width == 16 && height == 16 && pitch == 64);
        assert(pixels[0] == 0 && pixels[1] == 0 && pixels[2] == 255 && pixels[3] == 255);
    } else assert(pixels == NULL);
}

int main(void) {
    NvrDecoder decoder;
    AVCodecParameters *parameters = avcodec_parameters_alloc();
    assert(parameters);
    parameters->codec_type = AVMEDIA_TYPE_VIDEO;
    parameters->codec_id = AV_CODEC_ID_H264;
    parameters->width = parameters->height = 16;
    assert(nvr_decoder_open(&decoder, parameters) == 0);
    assert(decoder.hw_format == AV_PIX_FMT_NONE && !decoder.codec->hw_device_ctx);
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    int expected_attempts = 0;
    for (int i = 0; ; ++i) {
        const AVCodecHWConfig *hw = avcodec_get_hw_config(codec, i);
        if (!hw) break;
        if ((hw->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
            (hw->device_type == AV_HWDEVICE_TYPE_VAAPI || hw->device_type == AV_HWDEVICE_TYPE_CUDA))
            ++expected_attempts;
    }
    assert(device_attempts == expected_attempts);
    nvr_decoder_close(&decoder);

    parameters->codec_id = AV_CODEC_ID_RAWVIDEO;
    parameters->format = AV_PIX_FMT_RGB24;
    assert(nvr_decoder_open(&decoder, parameters) == 0);
    AVPacket *packet = av_packet_alloc();
    assert(packet && av_new_packet(packet, 16 * 16 * 3) == 0);
    for (int i = 0; i < packet->size; i += 3) {
        packet->data[i] = 255; packet->data[i + 1] = packet->data[i + 2] = 0;
    }
    packet->flags = AV_PKT_FLAG_KEY;
    assert(nvr_decoder_send(&decoder, packet) == 0);
    check_frame(&decoder, 1);
    assert(nvr_decoder_send(&decoder, packet) == 0);
    check_frame(&decoder, 0);

    /* An actual decode error on the hardware path must reopen in software. */
    decoder.hw_format = AV_PIX_FMT_VAAPI;
    AVPacket *bad = av_packet_alloc();
    assert(bad && av_new_packet(bad, 1) == 0);
    decoder.wait_keyframe = 1;
    /* Joining a live stream mid-GOP must not trigger a spurious GPU failure. */
    assert(nvr_decoder_send(&decoder, bad) == 0);
    assert(decoder.hw_format == AV_PIX_FMT_VAAPI && decoder.wait_keyframe);
    bad->flags = AV_PKT_FLAG_KEY;
    assert(nvr_decoder_send(&decoder, bad) < 0);
    assert(decoder.hw_format == AV_PIX_FMT_NONE && decoder.codec);
    assert(nvr_decoder_send(&decoder, packet) == 0);
    check_frame(&decoder, 1);
    av_packet_free(&bad);
    av_packet_free(&packet);
    avcodec_parameters_free(&parameters);
    nvr_decoder_close(&decoder);
    nvr_decoder_close(&decoder);
    return 0;
}
