#include <kinc/audio2/audio.h>

#include <alsa/asoundlib.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// apt-get install libasound2-dev

void (*a2_callback)(kinc_a2_buffer_t *buffer, int samples) = NULL;
kinc_a2_buffer_t a2_buffer;

pthread_t threadid;
bool audioRunning = false;
snd_pcm_t *playback_handle;
short buf[4096 * 4];

void copySample(void *buffer) {
	float value = *(float *)&a2_buffer.data[a2_buffer.read_location];
	a2_buffer.read_location += 4;
	if (a2_buffer.read_location >= a2_buffer.data_size)
		a2_buffer.read_location = 0;
	if (value != 0) {
		int a = 3;
		++a;
	}
	*(int16_t *)buffer = (int16_t)(value * 32767);
}

bool tryToRecover(snd_pcm_t *handle, int errorCode) {
	switch (-errorCode) {
	case EINTR:
	case EPIPE:
	case ESPIPE:
#if !defined(__FreeBSD__)
	case ESTRPIPE:
#endif
	{
		int recovered = snd_pcm_recover(playback_handle, errorCode, 1);

		if (recovered != 0) {
			fprintf(stderr, "unable to recover from ALSA error code=%i\n", errorCode);
			return false;
		}
		else {
			fprintf(stdout, "recovered from ALSA error code=%i\n", errorCode);
			return true;
		}
	}
	default:
		fprintf(stderr, "unhandled ALSA error code=%i\n", errorCode);
		return false;
	}
}

int playback_callback(snd_pcm_sframes_t nframes) {
	int err = 0;
	if (a2_callback != NULL) {
		a2_callback(&a2_buffer, nframes * 2);
		int ni = 0;
		while (ni < nframes) {
			int i = 0;
			for (; ni < nframes && i < 4096 * 2; ++i, ++ni) {
				copySample(&buf[i * 2]);
				copySample(&buf[i * 2 + 1]);
			}
			int err2;
			if ((err2 = snd_pcm_writei(playback_handle, buf, i)) < 0) {
				tryToRecover(playback_handle, err2);
			}
			err += err2;
		}
	}
	return err;
}

void* doAudio(void* arg) {
	snd_pcm_hw_params_t* hw_params;
	int err;

	if ((err = snd_pcm_open(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		fprintf(stderr, "cannot open audio device default (%s)\n", snd_strerror(err));
		return NULL;
	}

	if ((err = snd_pcm_set_params(playback_handle,
						  SND_PCM_FORMAT_S16_LE,
						  SND_PCM_ACCESS_RW_INTERLEAVED,
						  2,
						  44100,
						  1,
						  200000)) < 0) {
		fprintf(stderr, "cannot set audio device params (%s)\n", snd_strerror(err));
		return NULL;
	}

	while (audioRunning) {

		playback_callback(2048);

	}

	snd_pcm_drain(playback_handle);
	snd_pcm_close(playback_handle);
	return NULL;
}

static bool initialized = false;

void kinc_a2_init() {
	if (initialized) {
		return;
	}

	initialized = true;

	a2_buffer.read_location = 0;
	a2_buffer.write_location = 0;
	a2_buffer.data_size = 128 * 1024;
	a2_buffer.data = (uint8_t *)malloc(a2_buffer.data_size);

	audioRunning = true;
	pthread_create(&threadid, NULL, &doAudio, NULL);
}

void kinc_a2_update() {}

void kinc_a2_shutdown() {
	audioRunning = false;
}

void kinc_a2_set_callback(void (*kinc_a2_audio_callback)(kinc_a2_buffer_t *buffer, int samples)) {
	a2_callback = kinc_a2_audio_callback;
}
