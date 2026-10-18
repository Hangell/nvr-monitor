#ifndef NVR_MEDIA_H
#define NVR_MEDIA_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

typedef struct AVFormatContext AVFormatContext;
typedef struct AVStream AVStream;
typedef struct AVPacket AVPacket;

typedef enum {
    NVR_MEDIA_IDLE, NVR_MEDIA_WAITING, NVR_MEDIA_ACTIVE,
    NVR_MEDIA_STOPPING, NVR_MEDIA_SAVED, NVR_MEDIA_ERROR, NVR_MEDIA_CANCELLED
} NvrMediaState;

typedef struct {
    pthread_mutex_t mutex;
    atomic_int recording_state, screenshot_state;
    atomic_uint revision;
    char name[64];
    char recording_directory[512], screenshot_directory[512];
    char recording_path[1024], screenshot_path[1024];
    /* Output resources and timestamps belong exclusively to the camera worker. */
    AVFormatContext *output;
    FILE *file;
    int header_written;
    int64_t origin, last_dts, next_timestamp;
    unsigned packets;
} NvrMedia;

int nvr_media_init(NvrMedia *media, const char *name);
void nvr_media_destroy(NvrMedia *media);
int nvr_media_request_recording(NvrMedia *media, const char *directory);
void nvr_media_stop_recording(NvrMedia *media);
int nvr_media_request_screenshot(NvrMedia *media, const char *directory);
/* Called by the camera worker, never by the UI thread. */
void nvr_media_packet(NvrMedia *media, const AVStream *stream, const AVPacket *packet);
void nvr_media_frame(NvrMedia *media, const uint8_t *pixels, int width, int height, int pitch);
void nvr_media_end_stream(NvrMedia *media);

#endif
