#include "nvr/camera.h"
#include "nvr/decoder.h"
#include "nvr/logger.h"
#include "nvr/network.h"
#include "nvr/rtsp.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>

static atomic_int opens, resolutions;
static const atomic_int *session_cancel;
static int reads, decoded, resolver_failure, resolver_wait;
static char first_host[NVR_HOST_MAX], second_host[NVR_HOST_MAX];

void nvr_log(NvrLogLevel level, const char *format, ...) {
    (void)level; (void)format;
}

int nvr_network_resolve_mac(const char *mac, const char *hint, const atomic_int *cancel,
                            char *host, size_t size, char *error, size_t error_size) {
    assert(!strcmp(mac, "02:11:22:33:44:55"));
    assert(!strcmp(hint, "192.0.2.99"));
    int count = atomic_fetch_add(&resolutions, 1) + 1;
    while (resolver_wait && !atomic_load(cancel)) av_usleep(1000);
    if (resolver_failure || atomic_load(cancel)) {
        snprintf(error, error_size, "MAC indisponível");
        return -1;
    }
    snprintf(host, size, "%s", count == 1 ? "192.0.2.10" : "192.0.2.11");
    return 0;
}

int nvr_rtsp_open_interruptible(NvrRtspSession *session, const NvrCameraConfig *config,
                                int main_stream, const char *password,
                                const atomic_int *cancel) {
    (void)main_stream; (void)password;
    assert(cancel && !atomic_load(cancel));
    session_cancel = cancel;
    session->format = avformat_alloc_context();
    assert(session->format && avformat_new_stream(session->format, NULL));
    session->video_stream = 0;
    reads = 0;
    int count = atomic_load(&opens);
    snprintf(count ? second_host : first_host, NVR_HOST_MAX, "%s", config->host);
    atomic_fetch_add(&opens, 1);
    return 0;
}

void nvr_rtsp_close(NvrRtspSession *session) {
    avformat_free_context(session->format);
    session->format = NULL;
}

int av_read_frame(AVFormatContext *format, AVPacket *packet) {
    (void)format;
    if (!reads++) {
        assert(av_new_packet(packet, 1) == 0);
        packet->stream_index = 0;
        packet->flags = AV_PKT_FLAG_KEY;
        return 0;
    }
    /* A disconnect after one packet must still have displayed that packet. */
    assert(session_cancel);
    return AVERROR_EOF;
}

int nvr_decoder_open(NvrDecoder *decoder, const AVCodecParameters *parameters) {
    (void)decoder; (void)parameters;
    decoded = 0;
    return 0;
}
int nvr_decoder_send(NvrDecoder *decoder, const AVPacket *packet) {
    (void)decoder;
    assert(packet && packet->size == 1);
    decoded = 1;
    return 0;
}
int nvr_decoder_receive_rgba(NvrDecoder *decoder, uint8_t **pixels, int *width,
                            int *height, int *pitch, int convert) {
    (void)decoder;
    if (!decoded) return AVERROR(EAGAIN);
    decoded = 0;
    static uint8_t rgba[] = {1, 2, 3, 255};
    if (convert) {
        *pixels = rgba; *width = *height = 1; *pitch = 4;
    }
    return 0;
}
void nvr_decoder_close(NvrDecoder *decoder) { (void)decoder; }

static void wait_counter(atomic_int *counter, int minimum, int64_t timeout_us) {
    int64_t deadline = av_gettime_relative() + timeout_us;
    while (atomic_load(counter) < minimum && av_gettime_relative() < deadline)
        av_usleep(1000);
    assert(atomic_load(counter) >= minimum);
}

static void wait_first_frame(NvrCamera *camera) {
    int64_t deadline = av_gettime_relative() + 1000000;
    NvrVideoFrame frame = {0};
    while (!nvr_frame_queue_take_latest(&camera->frames, &frame) &&
           av_gettime_relative() < deadline) av_usleep(1000);
    assert(frame.pixels && frame.pixels[0] == 1);
    nvr_video_frame_free(&frame);
}

static void reset(void) {
    atomic_store(&opens, 0); atomic_store(&resolutions, 0);
    resolver_failure = resolver_wait = 0;
}

int main(void) {
    NvrCamera camera;
    NvrCameraConfig config = {.port = 554};
    strcpy(config.name, "Camera de teste");
    strcpy(config.host, "192.0.2.99");
    reset();
    assert(nvr_camera_init(&camera, &config) == 0);
    assert(nvr_camera_start(&camera) == 0);
    wait_first_frame(&camera);
    nvr_camera_destroy(&camera);
    assert(atomic_load(&resolutions) == 0 && !strcmp(first_host, config.host));

    reset();
    strcpy(config.mac_address, "02:11:22:33:44:55");
    assert(nvr_camera_init(&camera, &config) == 0);
    assert(nvr_camera_start(&camera) == 0);
    wait_first_frame(&camera);
    wait_counter(&opens, 2, 2500000);
    nvr_camera_destroy(&camera);
    assert(!strcmp(first_host, "192.0.2.10") && !strcmp(second_host, "192.0.2.11"));
    assert(!strcmp(camera.config.host, "192.0.2.99"));

    reset(); resolver_failure = 1;
    assert(nvr_camera_init(&camera, &config) == 0);
    assert(nvr_camera_start(&camera) == 0);
    wait_counter(&resolutions, 1, 1000000);
    nvr_camera_destroy(&camera);
    assert(atomic_load(&opens) == 0); /* Never send credentials to an unverified hint. */

    reset(); resolver_wait = 1;
    assert(nvr_camera_init(&camera, &config) == 0);
    assert(nvr_camera_start(&camera) == 0);
    wait_counter(&resolutions, 1, 1000000);
    int64_t start = av_gettime_relative();
    nvr_camera_destroy(&camera);
    assert(av_gettime_relative() - start < 1000000 && atomic_load(&opens) == 0);
    puts("Immediate display, DHCP reconnection, identity and cancellation checks passed");
    return 0;
}
