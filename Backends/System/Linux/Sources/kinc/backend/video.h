#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if defined(KINC_VIDEO_GSTREAMER)
#include <kinc/backend/video_gstreamer.h>
#else
typedef struct {
	int nothing;
} kinc_video_impl_t;

typedef struct kinc_internal_video_sound_stream {
	int nothing;
} kinc_internal_video_sound_stream_t;

void kinc_internal_video_sound_stream_init(kinc_internal_video_sound_stream_t *stream, int channel_count, int frequency);

void kinc_internal_video_sound_stream_destroy(kinc_internal_video_sound_stream_t *stream);

void kinc_internal_video_sound_stream_insert_data(kinc_internal_video_sound_stream_t *stream, float *data, int sample_count);

float kinc_internal_video_sound_stream_next_sample(kinc_internal_video_sound_stream_t *stream);

bool kinc_internal_video_sound_stream_ended(kinc_internal_video_sound_stream_t *stream);
#endif

#ifdef __cplusplus
}
#endif
