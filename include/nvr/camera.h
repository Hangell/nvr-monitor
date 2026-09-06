#ifndef NVR_CAMERA_H
#define NVR_CAMERA_H

#include <pthread.h>
#include <stdatomic.h>
#include "nvr/config.h"
#include "nvr/frame_queue.h"

typedef enum {
    NVR_CAMERA_STOPPED,
    NVR_CAMERA_CONNECTING,
    NVR_CAMERA_ONLINE,
    NVR_CAMERA_DISCONNECTED,
    NVR_CAMERA_RECONNECTING
} NvrCameraState;

typedef struct {
    NvrCameraConfig config;
    pthread_t thread;
    atomic_int stop_requested;
    atomic_int state;
    atomic_int main_stream_requested;
    atomic_int display_requested;
    NvrCameraState displayed_state; /* Main thread only. */
    NvrFrameQueue frames;
    int thread_started;
} NvrCamera;

int nvr_camera_init(NvrCamera *camera, const NvrCameraConfig *config);
int nvr_camera_start(NvrCamera *camera);
void nvr_camera_stop(NvrCamera *camera);
void nvr_camera_destroy(NvrCamera *camera);
NvrCameraState nvr_camera_state(const NvrCamera *camera);
void nvr_camera_request_main_stream(NvrCamera *camera, int enabled);
const char *nvr_camera_state_name(NvrCameraState state);

#endif
