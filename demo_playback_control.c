#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PCM_DEVICE "default"
#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_DURATION 5 // Default duration for recording
#define MAX_CHANNELS 2

int setup_pcm(snd_pcm_t **pcm_handle, int stream, int channels, unsigned int rate) {
    snd_pcm_hw_params_t *params;

    // Open PCM device
    if (snd_pcm_open(pcm_handle, PCM_DEVICE, stream, 0) < 0) {
        fprintf(stderr, "Error opening PCM device\n");
        return -1;
    }

    // Allocate parameters object
    snd_pcm_hw_params_alloca(&params);

    // Fill with default values
    snd_pcm_hw_params_any(*pcm_handle, params);

    // Set parameters
    snd_pcm_hw_params_set_access(*pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(*pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(*pcm_handle, params, channels);
    snd_pcm_hw_params_set_rate_near(*pcm_handle, params, &rate, 0);

    // Write parameters
    if (snd_pcm_hw_params(*pcm_handle, params) < 0) {
        fprintf(stderr, "Error setting PCM parameters\n");
        snd_pcm_close(*pcm_handle);
        return -1;
    }

    return 0;
}

void apply_volume(short *buffer, int size, float volume) {
    for (int i = 0; i < size / sizeof(short); i++) {
        int sample = (int)(buffer[i] * volume);
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        buffer[i] = (short)sample;
    }
}

int main() {
    snd_pcm_t *capture_handle, *playback_handle;
    int err;
    unsigned int rate = DEFAULT_SAMPLE_RATE;
    int duration = DEFAULT_DURATION;

    // Get user preferences
    int channels = MAX_CHANNELS;
    printf("Select channels (1 for Mono, 2 for Stereo): ");
    scanf("%d", &channels);
    if (channels < 1 || channels > MAX_CHANNELS) {
        fprintf(stderr, "Invalid channel selection. Defaulting to Stereo.\n");
        channels = MAX_CHANNELS;
    }

    printf("Select sample rate (44100 or 48000): ");
    scanf("%u", &rate);
    if (rate != 44100 && rate != 48000) {
        fprintf(stderr, "Invalid sample rate. Defaulting to 44100 Hz.\n");
        rate = DEFAULT_SAMPLE_RATE;
    }

    printf("Enter recording duration in seconds (default is %d): ", DEFAULT_DURATION);
    int input_duration;
    scanf("%d", &input_duration);
    if (input_duration > 0) {
        duration = input_duration;
    }

    int buffer_size = rate * duration * channels * sizeof(short);
    short *buffer = malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        return -1;
    }

    float volume;
    printf("Enter volume level (0.0 to 1.0): ");
    scanf("%f", &volume);
    if (volume < 0.0f || volume > 1.0f) {
        fprintf(stderr, "Volume must be between 0.0 and 1.0\n");
        free(buffer);
        return -1;
    }

    // Setup PCM for capturing
    if (setup_pcm(&capture_handle, SND_PCM_STREAM_CAPTURE, channels, rate) < 0) {
        free(buffer);
        return -1;
    }

    printf("Recording for %d seconds...\n", duration);
    
    if (snd_pcm_readi(capture_handle, buffer, rate * duration) < 0) {
        fprintf(stderr, "Error recording audio\n");
        snd_pcm_close(capture_handle);
        free(buffer);
        return -1;
    }

    snd_pcm_close(capture_handle);

    apply_volume(buffer, buffer_size, volume);

    // Setup PCM for playback
    if (setup_pcm(&playback_handle, SND_PCM_STREAM_PLAYBACK, channels, rate) < 0) {
        free(buffer);
        return -1;
    }

    // Loop playback option
    char loop_choice;
    do {
        printf("Playing back recorded audio at volume %.2f...\n", volume);
        
        int frames_to_play = rate * duration;
        int frames_written;

        while (frames_to_play > 0) {
            frames_written = snd_pcm_writei(playback_handle, buffer, frames_to_play);
            if (frames_written < 0) {
                fprintf(stderr, "Error writing audio: %s\n", snd_strerror(frames_written));
                if (frames_written == -EPIPE) {
                    // Buffer overrun
                    fprintf(stderr, "Buffer underrun occurred, preparing the device...\n");
                    snd_pcm_prepare(playback_handle);
                } else {
                    // Other error
                    snd_pcm_close(playback_handle);
                    free(buffer);
                    return -1;
                }
            }
            frames_to_play -= frames_written;
        }

        printf("Playback completed. Press 'q' to quit or any other key to replay: ");
        scanf(" %c", &loop_choice);
    } while (loop_choice != 'q');

    snd_pcm_drain(playback_handle);
    snd_pcm_close(playback_handle);
    free(buffer);

    return 0;
}


