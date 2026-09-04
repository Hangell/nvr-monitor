#ifndef NVR_FRAME_QUEUE_H
#define NVR_FRAME_QUEUE_H

#include <pthread.h>
#include <stdint.h>

typedef struct {
    uint8_t *pixels;
    int width;
    int height;
    int pitch;
    uint64_t serial;
} NvrVideoFrame;

typedef struct {
    pthread_mutex_t mutex;
    NvrVideoFrame frames[2];
    unsigned capacity;
    unsigned read_index;
    unsigned count;
    uint64_t next_serial;
} NvrFrameQueue;

int nvr_frame_queue_init(NvrFrameQueue *queue, unsigned capacity);
void nvr_frame_queue_destroy(NvrFrameQueue *queue);
int nvr_frame_queue_push(NvrFrameQueue *queue, const uint8_t *pixels,
                         int width, int height, int pitch);
int nvr_frame_queue_take_latest(NvrFrameQueue *queue, NvrVideoFrame *frame);
void nvr_video_frame_free(NvrVideoFrame *frame);

#endif
