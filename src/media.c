#include "nvr/media.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#ifdef _WIN32
#include <io.h>
#endif

static void set_state(NvrMedia *media, atomic_int *state, NvrMediaState value) {
    atomic_store(state, value);
    atomic_fetch_add(&media->revision, 1);
}

static int pending(int state) {
    return state == NVR_MEDIA_WAITING || state == NVR_MEDIA_ACTIVE || state == NVR_MEDIA_STOPPING;
}

int nvr_media_init(NvrMedia *media, const char *name) {
    if (!media || !name) return -1;
    memset(media, 0, sizeof(*media));
    snprintf(media->name, sizeof(media->name), "%s", name);
    atomic_init(&media->recording_state, NVR_MEDIA_IDLE);
    atomic_init(&media->screenshot_state, NVR_MEDIA_IDLE);
    atomic_init(&media->revision, 0);
    return pthread_mutex_init(&media->mutex, NULL) == 0 ? 0 : -1;
}

static int request(NvrMedia *media, atomic_int *state, char destination[512], const char *directory) {
    if (!directory || !directory[0] || strlen(directory) >= 512) return -1;
    pthread_mutex_lock(&media->mutex);
    int result = -1;
    if (!pending(atomic_load(state))) {
        strcpy(destination, directory);
        set_state(media, state, NVR_MEDIA_WAITING);
        result = 0;
    }
    pthread_mutex_unlock(&media->mutex);
    return result;
}

int nvr_media_request_recording(NvrMedia *media, const char *directory) {
    return request(media, &media->recording_state, media->recording_directory, directory);
}

int nvr_media_request_screenshot(NvrMedia *media, const char *directory) {
    return request(media, &media->screenshot_state, media->screenshot_directory, directory);
}

void nvr_media_stop_recording(NvrMedia *media) {
    pthread_mutex_lock(&media->mutex);
    if (pending(atomic_load(&media->recording_state)))
        set_state(media, &media->recording_state, NVR_MEDIA_STOPPING);
    pthread_mutex_unlock(&media->mutex);
}

static FILE *create_file(const char *directory, const char *name, const char *extension,
                         char path[1024]) {
    char safe_name[64];
    size_t used = 0;
    for (; name[used] && used < sizeof(safe_name)-1; ++used) {
        unsigned char c = (unsigned char)name[used];
        safe_name[used] = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ? (char)c : '_';
    }
    safe_name[used] = '\0';
    time_t now = time(NULL);
    struct tm local;
#ifdef _WIN32
    if (localtime_s(&local, &now)) return NULL;
#else
    if (!localtime_r(&now, &local)) return NULL;
#endif
    char stamp[32];
    if (!strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &local)) return NULL;
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        int length = snprintf(path, 1024, "%s/%s_%s_%lld_%u.%s", directory,
                              safe_name[0] ? safe_name : "camera", stamp,
                              (long long)av_gettime(), attempt, extension);
        if (length < 0 || length >= 1024) return NULL;
        int flags = O_CREAT | O_EXCL | O_RDWR;
#ifdef _WIN32
        flags |= O_BINARY;
#endif
        int fd = open(path, flags, 0600);
        if (fd < 0) {
            if (errno == EEXIST) continue;
            return NULL;
        }
        FILE *file = fdopen(fd, "w+b");
        if (!file) { close(fd); remove(path); }
        return file;
    }
    return NULL;
}

/* Custom AVIO retains the exclusively-created file, avoiding overwrite races. */
#if LIBAVFORMAT_VERSION_MAJOR >= 61
static int write_output(void *opaque, const uint8_t *buffer, int size) {
#else
static int write_output(void *opaque, uint8_t *buffer, int size) {
#endif
    FILE *file = opaque;
    return fwrite(buffer, 1, (size_t)size, file) == (size_t)size ? size : AVERROR(EIO);
}

static int64_t seek_output(void *opaque, int64_t offset, int whence) {
    FILE *file = opaque;
#ifdef _WIN32
#define FILE_SEEK _fseeki64
#define FILE_TELL _ftelli64
#else
#define FILE_SEEK fseeko
#define FILE_TELL ftello
#endif
    if (whence == AVSEEK_SIZE) {
        int64_t current = FILE_TELL(file);
        if (current < 0 || FILE_SEEK(file, 0, SEEK_END)) return AVERROR(EIO);
        int64_t size = FILE_TELL(file);
        if (FILE_SEEK(file, current, SEEK_SET)) return AVERROR(EIO);
        return size;
    }
    if (FILE_SEEK(file, offset, whence & ~AVSEEK_FORCE)) return AVERROR(EIO);
    return FILE_TELL(file);
#undef FILE_SEEK
#undef FILE_TELL
}

static void finish_recording(NvrMedia *media, int failed) {
    int has_packets = media->file && media->packets;
    if (media->output) {
        if (media->header_written && av_write_trailer(media->output) < 0) failed = 1;
        if (media->output->pb) {
            avio_flush(media->output->pb);
            if (media->output->pb->error < 0) failed = 1;
            av_freep(&media->output->pb->buffer);
            avio_context_free(&media->output->pb);
        }
        avformat_free_context(media->output);
        media->output = NULL;
    }
    if (media->file) {
        if (fclose(media->file)) failed = 1;
        media->file = NULL;
        if (!media->packets) remove(media->recording_path);
    }
    media->header_written = 0;
    pthread_mutex_lock(&media->mutex);
    set_state(media, &media->recording_state, failed ? NVR_MEDIA_ERROR :
              has_packets ? NVR_MEDIA_SAVED : NVR_MEDIA_CANCELLED);
    pthread_mutex_unlock(&media->mutex);
}

static int start_recording(NvrMedia *media, const AVStream *stream) {
    media->packets = 0;
    media->origin = AV_NOPTS_VALUE;
    media->last_dts = AV_NOPTS_VALUE;
    media->next_timestamp = 0;
    media->file = create_file(media->recording_directory, media->name, "mkv", media->recording_path);
    if (!media->file) return -1;
    if (avformat_alloc_output_context2(&media->output, NULL, "matroska", NULL) < 0 || !media->output) return -1;
    AVStream *output_stream = avformat_new_stream(media->output, NULL);
    if (!output_stream || avcodec_parameters_copy(output_stream->codecpar, stream->codecpar) < 0) return -1;
    output_stream->codecpar->codec_tag = 0;
    output_stream->time_base = stream->time_base;
    output_stream->avg_frame_rate = stream->avg_frame_rate;
    uint8_t *buffer = av_malloc(32768);
    if (!buffer) return -1;
    media->output->pb = avio_alloc_context(buffer, 32768, 1, media->file, NULL, write_output, seek_output);
    if (!media->output->pb) { av_free(buffer); return -1; }
    media->output->flags |= AVFMT_FLAG_CUSTOM_IO;
    if (avformat_write_header(media->output, NULL) < 0) return -1;
    media->header_written = 1;
    return 0;
}

void nvr_media_packet(NvrMedia *media, const AVStream *stream, const AVPacket *packet) {
    int state = atomic_load(&media->recording_state);
    if (state == NVR_MEDIA_STOPPING) { finish_recording(media, 0); return; }
    if (state != NVR_MEDIA_WAITING && state != NVR_MEDIA_ACTIVE) return;
    if (state == NVR_MEDIA_WAITING) {
        if (!(packet->flags & AV_PKT_FLAG_KEY)) return;
        if (start_recording(media, stream)) { finish_recording(media, 1); return; }
        /* A stop request arriving during file creation must not be lost. */
        int expected = NVR_MEDIA_WAITING;
        if (!atomic_compare_exchange_strong(&media->recording_state, &expected, NVR_MEDIA_ACTIVE)) {
            finish_recording(media, 0); return;
        }
        atomic_fetch_add(&media->revision, 1);
    }
    AVPacket *copy = av_packet_clone(packet);
    if (!copy) { finish_recording(media, 1); return; }
    int64_t timestamp = copy->dts != AV_NOPTS_VALUE ? copy->dts : copy->pts;
    if (timestamp == AV_NOPTS_VALUE) timestamp = media->next_timestamp;
    if (media->origin == AV_NOPTS_VALUE) media->origin = timestamp;
    int64_t duration = copy->duration;
    if (duration <= 0 && stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0)
        duration = av_rescale_q(1, av_inv_q(stream->avg_frame_rate), stream->time_base);
    if (duration <= 0) duration = 1;
    media->next_timestamp = timestamp + duration;
    copy->dts = timestamp - media->origin;
    copy->pts = copy->pts == AV_NOPTS_VALUE ? copy->dts : copy->pts - media->origin;
    copy->duration = duration;
    av_packet_rescale_ts(copy, stream->time_base, media->output->streams[0]->time_base);
    /* Preserve composition offsets while correcting duplicate/backward DTS. */
    if (media->last_dts != AV_NOPTS_VALUE && copy->dts <= media->last_dts) {
        int64_t shift = media->last_dts + 1 - copy->dts;
        copy->dts += shift;
        copy->pts += shift;
    }
    media->last_dts = copy->dts;
    copy->stream_index = 0;
    copy->pos = -1;
    int result = av_interleaved_write_frame(media->output, copy);
    av_packet_free(&copy);
    if (result < 0 || media->output->pb->error < 0) finish_recording(media, 1);
    else ++media->packets;
}

static int write_png(FILE *file, const uint8_t *pixels, int width, int height, int pitch) {
    const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!encoder || !pixels || width <= 0 || height <= 0 || width > INT_MAX/4 || pitch < width*4) return -1;
    AVCodecContext *codec = avcodec_alloc_context3(encoder);
    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    struct SwsContext *sws = NULL;
    int result = -1;
    if (!codec || !frame || !packet) goto done;
    codec->width = width;
    codec->height = height;
    codec->pix_fmt = AV_PIX_FMT_RGB24;
    codec->time_base = (AVRational){1, 1};
    if (avcodec_open2(codec, encoder, NULL) < 0) goto done;
    frame->format = codec->pix_fmt;
    frame->width = width;
    frame->height = height;
    if (av_frame_get_buffer(frame, 0) < 0) goto done;
    sws = sws_getContext(width, height, AV_PIX_FMT_BGRA, width, height, AV_PIX_FMT_RGB24,
                        SWS_POINT, NULL, NULL, NULL);
    if (!sws) goto done;
    const uint8_t *source[4] = {pixels};
    int strides[4] = {pitch};
    if (sws_scale(sws, source, strides, 0, height, frame->data, frame->linesize) != height) goto done;
    if (avcodec_send_frame(codec, frame) < 0 || avcodec_receive_packet(codec, packet) < 0) goto done;
    result = fwrite(packet->data, 1, (size_t)packet->size, file) == (size_t)packet->size ? 0 : -1;
done:
    sws_freeContext(sws);
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&codec);
    return result;
}

void nvr_media_frame(NvrMedia *media, const uint8_t *pixels, int width, int height, int pitch) {
    int expected = NVR_MEDIA_WAITING;
    if (!atomic_compare_exchange_strong(&media->screenshot_state, &expected, NVR_MEDIA_ACTIVE)) return;
    FILE *file = create_file(media->screenshot_directory, media->name, "png", media->screenshot_path);
    int result = file ? write_png(file, pixels, width, height, pitch) : -1;
    if (file && fclose(file)) result = -1;
    if (file && result) remove(media->screenshot_path);
    set_state(media, &media->screenshot_state, result ? NVR_MEDIA_ERROR : NVR_MEDIA_SAVED);
}

void nvr_media_end_stream(NvrMedia *media) {
    if (pending(atomic_load(&media->recording_state))) finish_recording(media, 0);
    if (pending(atomic_load(&media->screenshot_state))) set_state(media, &media->screenshot_state, NVR_MEDIA_ERROR);
}

void nvr_media_destroy(NvrMedia *media) {
    nvr_media_end_stream(media);
    pthread_mutex_destroy(&media->mutex);
}
