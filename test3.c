#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#define RATE 44100
#define CHANNELS 2
#define SECONDS 5
#define SAMPLE_SIZE (sizeof(short))
#define FRAME_SIZE (CHANNELS * SAMPLE_SIZE)
#define BUFFER_SIZE (RATE * SECONDS * FRAME_SIZE)

// Generate white noise
void generate_noise(short *buffer, size_t size, float volume) {
    for (size_t i = 0; i < size/SAMPLE_SIZE; i++) {
        float random = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        buffer[i] = (short)(random * volume * 32767.0f);
    }
}

// Process audio with simple amplification
void process_audio(short *buffer, size_t size, float gain) {
    for (size_t i = 0; i < size/SAMPLE_SIZE; i++) {
        float sample = buffer[i] * gain;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32767.0f) sample = -32767.0f;
        buffer[i] = (short)sample;
    }
}

// Configure ALSA device
int setup_alsa(snd_pcm_t **handle, char *device, snd_pcm_stream_t stream) {
    int err;
    snd_pcm_hw_params_t *hw_params;

    if ((err = snd_pcm_open(handle, device, stream, 0)) < 0) {
        fprintf(stderr, "Cannot open audio device %s: %s\n", device, snd_strerror(err));
        return err;
    }

    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(*handle, hw_params);
    snd_pcm_hw_params_set_access(*handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(*handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(*handle, hw_params, CHANNELS);
    snd_pcm_hw_params_set_rate_near(*handle, hw_params, (unsigned int *)&RATE, 0);

    if ((err = snd_pcm_hw_params(*handle, hw_params)) < 0) {
        fprintf(stderr, "Cannot set parameters: %s\n", snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_prepare(*handle)) < 0) {
        fprintf(stderr, "Cannot prepare audio interface: %s\n", snd_strerror(err));
        return err;
    }

    return 0;
}

int play_buffer(snd_pcm_t *handle, short *buffer, int frames) {
    int err;
    while (frames > 0) {
        err = snd_pcm_writei(handle, buffer, frames);
        if (err == -EPIPE) {
            snd_pcm_prepare(handle);
            continue;
        } else if (err < 0) {
            fprintf(stderr, "Write error: %s\n", snd_strerror(err));
            return err;
        }
        frames -= err;
        buffer += err * CHANNELS;
    }
    return 0;
}

int record_buffer(snd_pcm_t *handle, short *buffer, int frames) {
    int err;
    while (frames > 0) {
        err = snd_pcm_readi(handle, buffer, frames);
        if (err == -EPIPE) {
            snd_pcm_prepare(handle);
            continue;
        } else if (err < 0) {
            fprintf(stderr, "Read error: %s\n", snd_strerror(err));
            return err;
        }
        frames -= err;
        buffer += err * CHANNELS;
    }
    return 0;
}

int main() {
    snd_pcm_t *playback_handle = NULL;
    snd_pcm_t *capture_handle = NULL;
    short *play_buffer_data;
    short *capture_buffer_data;
    int err;

    srand(time(NULL));

    // Allocate buffers
    play_buffer_data = malloc(BUFFER_SIZE);
    capture_buffer_data = malloc(BUFFER_SIZE);
    if (!play_buffer_data || !capture_buffer_data) {
        fprintf(stderr, "Cannot allocate buffers\n");
        return 1;
    }

    // First setup and use playback
    if ((err = setup_alsa(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK)) < 0) {
        goto cleanup;
    }

    printf("Generating and playing noise...\n");
    generate_noise(play_buffer_data, BUFFER_SIZE, 0.1f);
    
    if ((err = play_buffer(playback_handle, play_buffer_data, RATE * SECONDS)) < 0) {
        goto cleanup;
    }
    
    // Make sure playback is complete
    snd_pcm_drain(playback_handle);
    snd_pcm_close(playback_handle);
    playback_handle = NULL;
    
    // Now setup and use capture
    printf("Setting up recording...\n");
    if ((err = setup_alsa(&capture_handle, "default", SND_PCM_STREAM_CAPTURE)) < 0) {
        goto cleanup;
    }

    printf("Recording for 5 seconds...\n");
    if ((err = record_buffer(capture_handle, capture_buffer_data, RATE * SECONDS)) < 0) {
        goto cleanup;
    }

    // Close capture device
    snd_pcm_drain(capture_handle);
    snd_pcm_close(capture_handle);
    capture_handle = NULL;

    // Reopen playback for recorded audio
    if ((err = setup_alsa(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK)) < 0) {
        goto cleanup;
    }

    printf("Playing back recording...\n");
    process_audio(capture_buffer_data, BUFFER_SIZE, 1.2f);
    
    if ((err = play_buffer(playback_handle, capture_buffer_data, RATE * SECONDS)) < 0) {
        goto cleanup;
    }

cleanup:
    if (playback_handle) {
        snd_pcm_drain(playback_handle);
        snd_pcm_close(playback_handle);
    }
    if (capture_handle) {
        snd_pcm_drain(capture_handle);
        snd_pcm_close(capture_handle);
    }
    free(play_buffer_data);
    free(capture_buffer_data);

    return err < 0 ? 1 : 0;
}
