#include "nvr/rtsp.h"
#include "nvr/logger.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

int nvr_rtsp_percent_encode(const char *input, char *output, size_t size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t used = 0;
    if (!input || !output || !size) return -1;
    for (const unsigned char *p = (const unsigned char *)input; *p; ++p) {
        int safe = isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~';
        size_t needed = safe ? 1 : 3;
        if (used + needed >= size) return -1;
        if (safe) output[used++] = (char)*p;
        else {
            output[used++] = '%';
            output[used++] = hex[*p >> 4];
            output[used++] = hex[*p & 15];
        }
    }
    output[used] = '\0';
    return 0;
}

int nvr_rtsp_build_uri(const NvrCameraConfig *config, int main_stream,
                       const char *password, char *uri, size_t size) {
    char user[NVR_USER_MAX * 3], pass[NVR_ENV_MAX * 3];
    if (!config || !uri || !size) return -1;
    if (nvr_rtsp_percent_encode(config->username, user, sizeof(user)) != 0 ||
        nvr_rtsp_percent_encode(password ? password : "", pass, sizeof(pass)) != 0) return -1;
    const char *path = main_stream ? config->main_path : config->grid_path;
    while (*path == '/') path++;
    int n;
    if (config->username[0])
        n = snprintf(uri, size, "rtsp://%s:%s@%s:%u/%s", user, pass,
                     config->host, config->port, path);
    else
        n = snprintf(uri, size, "rtsp://%s:%u/%s", config->host, config->port, path);
    return n >= 0 && (size_t)n < size ? 0 : -1;
}

#ifndef NVR_RTSP_URI_ONLY
#include <libavformat/avformat.h>
#include <libavutil/dict.h>

int nvr_rtsp_open(NvrRtspSession *session, const NvrCameraConfig *config,
                  int main_stream, const char *password) {
    char uri[1024];
    AVDictionary *options = NULL;
    if (!session || nvr_rtsp_build_uri(config, main_stream, password, uri, sizeof(uri))) return -1;
    memset(session, 0, sizeof(*session));
    session->video_stream = -1;
    av_dict_set(&options, "rtsp_transport", nvr_transport_name(config->transport), 0);
    av_dict_set(&options, "timeout", "5000000", 0);
    av_dict_set(&options, "rw_timeout", "5000000", 0);
    av_dict_set(&options, "fflags", "nobuffer", 0);
    av_dict_set(&options, "flags", "low_delay", 0);
    int result = avformat_open_input(&session->format, uri, NULL, &options);
    av_dict_free(&options);
    if (result < 0) return result;
    result = avformat_find_stream_info(session->format, NULL);
    if (result < 0) { nvr_rtsp_close(session); return result; }
    session->video_stream = av_find_best_stream(session->format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (session->video_stream < 0) { result = session->video_stream; nvr_rtsp_close(session); return result; }
    return 0;
}

void nvr_rtsp_close(NvrRtspSession *session) {
    if (!session) return;
    avformat_close_input(&session->format);
    session->video_stream = -1;
}
#else
int nvr_rtsp_open(NvrRtspSession *s, const NvrCameraConfig *c, int m, const char *p) {
    (void)s; (void)c; (void)m; (void)p; return -1;
}
void nvr_rtsp_close(NvrRtspSession *s) { (void)s; }
#endif
