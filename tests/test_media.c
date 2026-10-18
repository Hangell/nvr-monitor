#include "nvr/media.h"
#include "test_temp_file.h"
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

static void check_image(const char *path) {
    AVFormatContext *input = NULL;
    assert(avformat_open_input(&input, path, NULL, NULL) == 0);
    assert(avformat_find_stream_info(input, NULL) >= 0);
    AVCodecContext *decoder = avcodec_alloc_context3(avcodec_find_decoder(AV_CODEC_ID_PNG));
    assert(decoder);
    assert(avcodec_parameters_to_context(decoder, input->streams[0]->codecpar) == 0);
    assert(avcodec_open2(decoder, avcodec_find_decoder(AV_CODEC_ID_PNG), NULL) == 0);
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    assert(packet && frame && av_read_frame(input, packet) == 0);
    assert(avcodec_send_packet(decoder, packet) == 0);
    assert(avcodec_receive_frame(decoder, frame) == 0);
    assert(frame->width == 2 && frame->height == 2 && frame->format == AV_PIX_FMT_RGB24);
    assert(frame->data[0][0] == 255 && frame->data[0][1] == 0 && frame->data[0][2] == 0);
    assert(frame->data[0][3] == 0 && frame->data[0][4] == 255 && frame->data[0][5] == 0);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&decoder);
    avformat_close_input(&input);
}

static int check_recording(const char *path) {
    AVFormatContext *input = NULL;
    assert(avformat_open_input(&input, path, NULL, NULL) == 0);
    assert(avformat_find_stream_info(input, NULL) >= 0);
    assert(input->nb_streams == 1);
    const AVCodec *codec = avcodec_find_decoder(input->streams[0]->codecpar->codec_id);
    AVCodecContext *decoder = avcodec_alloc_context3(codec);
    assert(decoder && avcodec_parameters_to_context(decoder, input->streams[0]->codecpar) == 0);
    assert(avcodec_open2(decoder, codec, NULL) == 0);
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int frames = 0, packets = 0;
    int64_t last_dts = AV_NOPTS_VALUE;
    assert(packet && frame);
    while (av_read_frame(input, packet) >= 0) {
        if (!packets) assert(packet->flags & AV_PKT_FLAG_KEY);
        if (packet->dts != AV_NOPTS_VALUE) {
            assert(last_dts == AV_NOPTS_VALUE || packet->dts > last_dts);
            last_dts = packet->dts;
        }
        assert(avcodec_send_packet(decoder, packet) == 0);
        while (avcodec_receive_frame(decoder, frame) == 0) {
            assert(frame->width == 64 && frame->height == 48);
            frames++;
        }
        packets++;
        av_packet_unref(packet);
    }
    assert(avcodec_send_packet(decoder, NULL) == 0);
    while (avcodec_receive_frame(decoder, frame) == 0) frames++;
    assert(packets > 0);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&decoder);
    avformat_close_input(&input);
    return frames;
}

int main(void) {
    char temporary[NVR_TEST_PATH_MAX], directory[NVR_TEST_PATH_MAX];
    nvr_test_temp_file(temporary);
    strcpy(directory, temporary);
    char *slash = strrchr(directory, '/');
#ifdef _WIN32
    char *backslash = strrchr(directory, '\\');
    if (!slash || (backslash && backslash > slash)) slash = backslash;
#endif
    assert(slash);
    *slash = '\0';
    NvrMedia media;
    assert(nvr_media_init(&media, "../Camera / test") == 0);
    const uint8_t pixels[16] = {0,0,255,255, 0,255,0,255, 255,0,0,255, 255,255,255,255};
    assert(nvr_media_request_screenshot(&media, directory) == 0);
    assert(nvr_media_request_screenshot(&media, directory) == -1);
    nvr_media_frame(&media, pixels, 2, 2, 8);
    assert(atomic_load(&media.screenshot_state) == NVR_MEDIA_SAVED);
    char first_image[1024];
    strcpy(first_image, media.screenshot_path);
    check_image(first_image);
    assert(nvr_media_request_screenshot(&media, directory) == 0);
    nvr_media_frame(&media, pixels, 2, 2, 8);
    assert(atomic_load(&media.screenshot_state) == NVR_MEDIA_SAVED);
    assert(strcmp(first_image, media.screenshot_path));
    check_image(first_image); /* Repeated captures never overwrite previous files. */
    remove(first_image);
    remove(media.screenshot_path);
    assert(nvr_media_request_screenshot(&media, temporary) == 0);
    nvr_media_frame(&media, pixels, 2, 2, 8);
    assert(atomic_load(&media.screenshot_state) == NVR_MEDIA_ERROR);

    const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    AVCodecContext *codec = avcodec_alloc_context3(encoder);
    assert(codec);
    codec->width = 64; codec->height = 48;
    codec->pix_fmt = AV_PIX_FMT_YUV420P;
    codec->time_base = (AVRational){1,25};
    codec->framerate = (AVRational){25,1};
    codec->gop_size = 5;
    codec->max_b_frames = 2;
    codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    assert(avcodec_open2(codec, encoder, NULL) == 0);
    AVFormatContext *source = avformat_alloc_context();
    AVStream *stream = avformat_new_stream(source, NULL);
    assert(stream && avcodec_parameters_from_context(stream->codecpar, codec) == 0);
    stream->time_base = codec->time_base;
    stream->avg_frame_rate = codec->framerate;
    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc(), *first_packet = NULL;
    assert(frame && packet);
    frame->width = 64; frame->height = 48; frame->format = codec->pix_fmt;
    assert(av_frame_get_buffer(frame, 0) == 0);
    assert(nvr_media_request_recording(&media, directory) == 0);
    assert(nvr_media_request_recording(&media, directory) == -1);
    for (int i = 0; i <= 12; ++i) {
        if (i < 12) {
            assert(av_frame_make_writable(frame) == 0);
            for (int plane = 0; plane < 3; ++plane)
                for (int row = 0; row < (plane ? 24 : 48); ++row)
                    memset(frame->data[plane]+row*frame->linesize[plane], plane ? 128 : 32+i*8, plane ? 32 : 64);
            frame->pts = i;
        }
        assert(avcodec_send_frame(codec, i < 12 ? frame : NULL) == 0);
        while (avcodec_receive_packet(codec, packet) == 0) {
            if (packet->pts != AV_NOPTS_VALUE) packet->pts += 9000;
            if (packet->dts != AV_NOPTS_VALUE) packet->dts += 9000;
            if (!first_packet) {
                first_packet = av_packet_clone(packet);
                assert(first_packet);
                packet->flags &= ~AV_PKT_FLAG_KEY;
                nvr_media_packet(&media, stream, packet);
                assert(atomic_load(&media.recording_state) == NVR_MEDIA_WAITING);
                packet->flags |= AV_PKT_FLAG_KEY;
            }
            nvr_media_packet(&media, stream, packet);
            assert(atomic_load(&media.recording_state) == NVR_MEDIA_ACTIVE);
            av_packet_unref(packet);
        }
    }
    nvr_media_stop_recording(&media);
    assert(atomic_load(&media.recording_state) == NVR_MEDIA_STOPPING);
    nvr_media_packet(&media, stream, first_packet);
    assert(atomic_load(&media.recording_state) == NVR_MEDIA_SAVED);
    assert(check_recording(media.recording_path) == 12);
    remove(media.recording_path);
    assert(nvr_media_request_recording(&media, directory) == 0);
    nvr_media_stop_recording(&media);
    nvr_media_end_stream(&media);
    assert(atomic_load(&media.recording_state) == NVR_MEDIA_CANCELLED);
    assert(nvr_media_request_recording(&media, temporary) == 0);
    nvr_media_packet(&media, stream, first_packet);
    assert(atomic_load(&media.recording_state) == NVR_MEDIA_ERROR);
    assert(nvr_media_request_recording(&media, directory) == 0);
    nvr_media_packet(&media, stream, first_packet);
    assert(nvr_media_request_screenshot(&media, directory) == 0);
    nvr_media_end_stream(&media); /* Disconnect/quality switch finalizes the file. */
    assert(atomic_load(&media.recording_state) == NVR_MEDIA_SAVED);
    assert(atomic_load(&media.screenshot_state) == NVR_MEDIA_ERROR);
    assert(check_recording(media.recording_path) == 1);
    remove(media.recording_path);
#ifndef _WIN32
    /* A filesystem write failure must never be reported as a saved recording. */
    int full = open("/dev/full", O_WRONLY);
    if (full >= 0) {
        assert(nvr_media_request_recording(&media, directory) == 0);
        nvr_media_packet(&media, stream, first_packet);
        assert(atomic_load(&media.recording_state) == NVR_MEDIA_ACTIVE);
        assert(dup2(full, fileno(media.file)) >= 0);
        close(full);
        nvr_media_end_stream(&media);
        assert(atomic_load(&media.recording_state) == NVR_MEDIA_ERROR);
        remove(media.recording_path);
    }
#endif
    av_packet_free(&first_packet);
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&codec);
    avformat_free_context(source);
    nvr_media_destroy(&media);
    remove(temporary);
    puts("PNG colors, unique files, MKV playback with B-frames, stop and failure checks passed");
    return 0;
}
