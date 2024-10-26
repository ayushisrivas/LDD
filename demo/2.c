#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PCM_DEVICE "default"
#define SAMPLE_RATE 44100
#define DURATION 5  // Duration in seconds
#define CHANNELS 2  // Stereo

int setup_pcm(snd_pcm_t **pcm_handle, int stream) {
    snd_pcm_hw_params_t *params;
    unsigned int rate = SAMPLE_RATE;

    // Open PCM device
    if (snd_pcm_open(pcm_handle, PCM_DEVICE, stream, 0) < 0) {
        printf("Error opening PCM device\n");
        return -1;
    }

    // Allocate parameters object
    snd_pcm_hw_params_alloca(&params);

    // Fill with default values
    snd_pcm_hw_params_any(*pcm_handle, params);

    // Set parameters
    snd_pcm_hw_params_set_access(*pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(*pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(*pcm_handle, params, CHANNELS);
    snd_pcm_hw_params_set_rate_near(*pcm_handle, params, &rate, 0);

    // Write parameters
    if (snd_pcm_hw_params(*pcm_handle, params) < 0) {
        printf("Error setting PCM parameters\n");
        return -1;
    }

    return 0;
}

int main() {
    snd_pcm_t *capture_handle, *playback_handle;
    int err;

    // Allocate buffer for audio data
    int buffer_size = SAMPLE_RATE * DURATION * CHANNELS * sizeof(short);
    short *buffer = malloc(buffer_size);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        return -1;
    }

    // Setup PCM for capturing
    if (setup_pcm(&capture_handle, SND_PCM_STREAM_CAPTURE) < 0) {
        free(buffer);
        return -1;
    }

    printf("Recording for %d seconds...\n", DURATION);
    
    // Record audio
    if (snd_pcm_readi(capture_handle, buffer, SAMPLE_RATE * DURATION) < 0) {
        printf("Error recording audio\n");
        snd_pcm_close(capture_handle);
        free(buffer);
        return -1;
    }

    // Close capture device
    snd_pcm_close(capture_handle);

    // Setup PCM for playback
    if (setup_pcm(&playback_handle, SND_PCM_STREAM_PLAYBACK) < 0) {
        free(buffer);
        return -1;
    }

    printf("Playing back recorded audio...\n");
    
    // Play audio
    if (snd_pcm_writei(playback_handle, buffer, SAMPLE_RATE * DURATION) < 0) {
        printf("Error playing audio\n");
        snd_pcm_close(playback_handle);
        free(buffer);
        return -1;
    }

    // Clean up
    snd_pcm_drain(playback_handle);
    snd_pcm_close(playback_handle);
    free(buffer);

    return 0;
}

