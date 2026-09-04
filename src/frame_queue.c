#include "nvr/frame_queue.h"
#include <stdlib.h>
#include <string.h>

void nvr_video_frame_free(NvrVideoFrame *frame) {
    if (!frame) return;
    free(frame->pixels);
    memset(frame, 0, sizeof(*frame));
}

int nvr_frame_queue_init(NvrFrameQueue *queue, unsigned capacity) {
    if (!queue || capacity < 1 || capacity > 2) return -1;
    memset(queue, 0, sizeof(*queue));
    queue->capacity = capacity;
    return pthread_mutex_init(&queue->mutex, NULL) == 0 ? 0 : -1;
}

void nvr_frame_queue_destroy(NvrFrameQueue *queue) {
    if (!queue) return;
    pthread_mutex_lock(&queue->mutex);
    for (unsigned i = 0; i < 2; ++i) nvr_video_frame_free(&queue->frames[i]);
    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);
}

int nvr_frame_queue_push(NvrFrameQueue *queue, const uint8_t *pixels,
                         int width, int height, int pitch) {
    if (!queue || !pixels || width <= 0 || height <= 0 || pitch <= 0) return -1;
    size_t bytes = (size_t)pitch * (size_t)height;
    uint8_t *copy = malloc(bytes);
    if (!copy) return -1;
    memcpy(copy, pixels, bytes);

    pthread_mutex_lock(&queue->mutex);
    unsigned index;
    if (queue->count == queue->capacity) {
        index = queue->read_index;
        queue->read_index = (queue->read_index + 1) % queue->capacity;
        nvr_video_frame_free(&queue->frames[index]);
    } else {
        index = (queue->read_index + queue->count) % queue->capacity;
        queue->count++;
    }
    queue->frames[index] = (NvrVideoFrame){copy, width, height, pitch, ++queue->next_serial};
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

int nvr_frame_queue_take_latest(NvrFrameQueue *queue, NvrVideoFrame *frame) {
    if (!queue || !frame) return 0;
    pthread_mutex_lock(&queue->mutex);
    if (!queue->count) {
        pthread_mutex_unlock(&queue->mutex);
        return 0;
    }
    while (queue->count > 1) {
        nvr_video_frame_free(&queue->frames[queue->read_index]);
        queue->read_index = (queue->read_index + 1) % queue->capacity;
        queue->count--;
    }
    *frame = queue->frames[queue->read_index];
    memset(&queue->frames[queue->read_index], 0, sizeof(queue->frames[0]));
    queue->read_index = (queue->read_index + 1) % queue->capacity;
    queue->count = 0;
    pthread_mutex_unlock(&queue->mutex);
    return 1;
}
