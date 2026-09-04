#include "nvr/camera.h"
#include "nvr/decoder.h"
#include "nvr/logger.h"
#include "nvr/rtsp.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>

static int sleep_interruptible(NvrCamera *camera, unsigned seconds) {
    for (unsigned i = 0; i < seconds * 10 && !atomic_load(&camera->stop_requested); ++i) {
        struct timespec delay = {0, 100000000}; nanosleep(&delay, NULL);
    }
    return atomic_load(&camera->stop_requested);
}

static void *camera_worker(void *data) {
    NvrCamera *camera = data;
    unsigned backoff = 1;
    while (!atomic_load(&camera->stop_requested)) {
        const char *password = camera->config.password_env[0] ? getenv(camera->config.password_env) : "";
        if (!password) {
            nvr_log(NVR_LOG_ERROR, "Câmera '%s': variável de senha não definida", camera->config.name);
            atomic_store(&camera->state, NVR_CAMERA_DISCONNECTED);
            if (sleep_interruptible(camera, backoff)) break;
            if (backoff < 30) backoff *= 2;
            continue;
        }
        atomic_store(&camera->state, backoff == 1 ? NVR_CAMERA_CONNECTING : NVR_CAMERA_RECONNECTING);
        NvrRtspSession session;
        int selected_stream = atomic_load(&camera->main_stream_requested);
        int result = nvr_rtsp_open(&session, &camera->config, selected_stream, password);
        if (result < 0) {
            char reason[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(result, reason, sizeof(reason));
            nvr_log(NVR_LOG_WARN,
                    "Câmera '%s': conexão falhou (%s); nova tentativa em %u s",
                    camera->config.name, reason, backoff);
            atomic_store(&camera->state, NVR_CAMERA_DISCONNECTED);
            if (sleep_interruptible(camera, backoff)) break;
            if (backoff < 30) backoff = backoff * 2 > 30 ? 30 : backoff * 2;
            continue;
        }
        NvrDecoder decoder;
        result = nvr_decoder_open(&decoder, session.format->streams[session.video_stream]->codecpar);
        if (result < 0) {
            nvr_log(NVR_LOG_WARN, "Câmera '%s': não foi possível abrir o decodificador", camera->config.name);
            nvr_rtsp_close(&session);
            atomic_store(&camera->state, NVR_CAMERA_DISCONNECTED);
            if (sleep_interruptible(camera, backoff)) break;
            if (backoff < 30) backoff = backoff * 2 > 30 ? 30 : backoff * 2;
            continue;
        }
        atomic_store(&camera->state, NVR_CAMERA_ONLINE); backoff = 1;
        AVPacket *packet = av_packet_alloc();
        while (packet && !atomic_load(&camera->stop_requested) &&
               selected_stream == atomic_load(&camera->main_stream_requested) &&
               av_read_frame(session.format, packet) >= 0) {
            if (packet->stream_index == session.video_stream && nvr_decoder_send(&decoder, packet) >= 0) {
                for (;;) {
                    uint8_t *pixels; int width, height, pitch;
                    result = nvr_decoder_receive_rgba(&decoder, &pixels, &width, &height, &pitch);
                    if (result < 0) break;
                    nvr_frame_queue_push(&camera->frames, pixels, width, height, pitch);
                }
            }
            av_packet_unref(packet);
        }
        av_packet_free(&packet); nvr_decoder_close(&decoder); nvr_rtsp_close(&session);
        if (!atomic_load(&camera->stop_requested)) {
            atomic_store(&camera->state, NVR_CAMERA_RECONNECTING);
            nvr_log(NVR_LOG_WARN, "Câmera '%s': stream interrompido; reconectando", camera->config.name);
            sleep_interruptible(camera, backoff);
        }
    }
    atomic_store(&camera->state, NVR_CAMERA_STOPPED);
    return NULL;
}

int nvr_camera_init(NvrCamera *camera, const NvrCameraConfig *config) {
    if (!camera || !config) return -1;
    memset(camera, 0, sizeof(*camera)); camera->config = *config;
    atomic_init(&camera->stop_requested, 0); atomic_init(&camera->state, NVR_CAMERA_STOPPED);
    atomic_init(&camera->main_stream_requested, 0);
    return nvr_frame_queue_init(&camera->frames, 2);
}
int nvr_camera_start(NvrCamera *camera) {
    if (!camera || camera->thread_started) return -1;
    atomic_store(&camera->stop_requested, 0);
    if (pthread_create(&camera->thread, NULL, camera_worker, camera)) return -1;
    camera->thread_started = 1; return 0;
}
void nvr_camera_stop(NvrCamera *camera) {
    if (!camera || !camera->thread_started) return;
    atomic_store(&camera->stop_requested, 1); pthread_join(camera->thread, NULL); camera->thread_started = 0;
}
void nvr_camera_destroy(NvrCamera *camera) { if (camera) { nvr_camera_stop(camera); nvr_frame_queue_destroy(&camera->frames); } }
NvrCameraState nvr_camera_state(const NvrCamera *camera) { return camera ? atomic_load(&camera->state) : NVR_CAMERA_STOPPED; }
void nvr_camera_request_main_stream(NvrCamera *camera, int enabled) {
    if (camera) atomic_store(&camera->main_stream_requested, enabled != 0);
}
const char *nvr_camera_state_name(NvrCameraState state) {
    static const char *names[] = {"parada", "conectando", "online", "desconectada", "reconectando"};
    return state >= NVR_CAMERA_STOPPED && state <= NVR_CAMERA_RECONNECTING ? names[state] : "desconhecida";
}
