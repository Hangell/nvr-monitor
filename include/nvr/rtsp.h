#ifndef NVR_RTSP_H
#define NVR_RTSP_H

#include <stddef.h>
#include "nvr/config.h"

typedef struct AVFormatContext AVFormatContext;

typedef struct {
    AVFormatContext *format;
    int video_stream;
} NvrRtspSession;

int nvr_rtsp_percent_encode(const char *input, char *output, size_t output_size);
int nvr_rtsp_build_uri(const NvrCameraConfig *config, int main_stream,
                       const char *password, char *uri, size_t uri_size);
int nvr_rtsp_open(NvrRtspSession *session, const NvrCameraConfig *config,
                  int main_stream, const char *password);
void nvr_rtsp_close(NvrRtspSession *session);

#endif
