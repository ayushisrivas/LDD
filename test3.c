#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>

#define RATE 44100
#define CHANNELS 2
#define SECONDS 5
#define SAMPLE_SIZE (sizeof(short))
#define FRAME_SIZE (CHANNELS * SAMPLE_SIZE)
#define BUFFER_SIZE (RATE * SECONDS * FRAME_SIZE)
#define PERIOD_SIZE 4096

// Global volume control
static float master_volume = 1.0f;

// Function to handle ALSA xrun (under/overflow) recovery
static int xrun_recovery(snd_pcm_t *handle, int err) {
    if (err == -EPIPE) {    // under-run
        err = snd_pcm_prepare(handle);
        if (err < 0) {
            fprintf(stderr, "Can't recover from underrun: %s\n", snd_strerror(err));
            return err;
        }
    } else if (err == -ESTRPIPE) {    // system suspended
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0) {
                fprintf(stderr, "Can't recover from suspend: %s\n", snd_strerror(err));
                return err;
            }
        }
    }
    return err;
}

// Function to safely write to PCM device
static int write_pcm(snd_pcm_t *handle, short *buffer, snd_pcm_uframes_t frames) {
    snd_pcm_uframes_t frames_left = frames;
    short *buf = buffer;
    
    while (frames_left > 0) {
        int err = snd_pcm_writei(handle, buf, frames_left);
        if (err < 0) {
            if ((err = xrun_recovery(handle, err)) < 0) {
                fprintf(stderr, "Write error: %s\n", snd_strerror(err));
                return err;
            }
            continue;
        }
        
        frames_left -= err;
        buf += err * CHANNELS;
    }
    return 0;
}

// Function to safely read from PCM device
static int read_pcm(snd_pcm_t *handle, short *buffer, snd_pcm_uframes_t frames) {
    snd_pcm_uframes_t frames_left = frames;
    short *buf = buffer;
    
    while (frames_left > 0) {
        int err = snd_pcm_readi(handle, buf, frames_left);
        if (err < 0) {
            if ((err = xrun_recovery(handle, err)) < 0) {
                fprintf(stderr, "Read error: %s\n", snd_strerror(err));
                return err;
            }
            continue;
        }
        
        frames_left -= err;
        buf += err * CHANNELS;
    }
    return 0;
}

// Function to generate white noise
void generate_noise(short *buffer, size_t buffer_size, float volume) {
    volume *= master_volume;  // Apply master volume
    for (size_t i = 0; i < buffer_size / SAMPLE_SIZE; i++) {
        float random = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        buffer[i] = (short)(random * volume * 32767.0f);
    }
}

// Function to process audio (simple amplification with volume control)
void process_audio(short *buffer, size_t buffer_size, float gain) {
    gain *= master_volume;  // Apply master volume
    for (size_t i = 0; i < buffer_size / SAMPLE_SIZE; i++) {
        float sample = buffer[i];
        sample *= gain;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32767.0f) sample = -32767.0f;
        buffer[i] = (short)sample;
    }
}

// Configure ALSA device
static int configure_alsa(snd_pcm_t *handle, unsigned int rate, unsigned int channels) {
    snd_pcm_hw_params_t *hw_params;
    int err;

    snd_pcm_hw_params_alloca(&hw_params);
    
    if ((err = snd_pcm_hw_params_any(handle, hw_params)) < 0) {
        fprintf(stderr, "Cannot initialize hardware parameter structure: %s\n", snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "Cannot set access type: %s\n", snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
        fprintf(stderr, "Cannot set sample format: %s\n", snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_set_channels(handle, hw_params, channels)) < 0) {
        fprintf(stderr, "Cannot set channel count: %s\n", snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, 0)) < 0) {
        fprintf(stderr, "Cannot set sample rate: %s\n", snd_strerror(err));
        return err;
    }

    // Set period size
    snd_pcm_uframes_t period_size = PERIOD_SIZE;
    if ((err = snd_pcm_hw_params_set_period_size_near(handle, hw_params, &period_size, 0)) < 0) {
        fprintf(stderr, "Cannot set period size: %s\n", snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params(handle, hw_params)) < 0) {
        fprintf(stderr, "Cannot set parameters: %s\n", snd_strerror(err));
        return err;
    }

    return 0;
}

// Function to set volume (0.0 to 1.0)
void set_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    master_volume = volume;
    printf("Volume set to %.1f%%\n", volume * 100);
}

int main() {
    snd_pcm_t *playback_handle, *capture_handle;
    int err;
    short *buffer, *capture_buffer;

    // Allocate buffers
    buffer = (short *)malloc(BUFFER_SIZE);
    capture_buffer = (short *)malloc(BUFFER_SIZE);
    if (!buffer || !capture_buffer) {
        fprintf(stderr, "Buffer allocation failed\n");
        return 1;
    }

    // Open playback device
    if ((err = snd_pcm_open(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "Playback open error: %s\n", snd_strerror(err));
        return 1;
    }

    // Open capture device
    if ((err = snd_pcm_open(&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "Capture open error: %s\n", snd_strerror(err));
        snd_pcm_close(playback_handle);
        return 1;
    }

    // Configure devices
    if ((err = configure_alsa(playback_handle, RATE, CHANNELS)) < 0 ||
        (err = configure_alsa(capture_handle, RATE, CHANNELS)) < 0) {
        snd_pcm_close(playback_handle);
        snd_pcm_close(capture_handle);
        return 1;
    }

    // Set initial volume to 50%
    set_volume(0.5f);

    printf("Generating and playing noise...\n");
    generate_noise(buffer, BUFFER_SIZE, 0.1f);
    if ((err = write_pcm(playback_handle, buffer, RATE * SECONDS)) < 0) {
        goto cleanup;
    }

    // Wait for playback to complete
    snd_pcm_drain(playback_handle);
    
    printf("Recording for 5 seconds...\n");
    // Prepare capture
    snd_pcm_prepare(capture_handle);
    
    if ((err = read_pcm(capture_handle, capture_buffer, RATE * SECONDS)) < 0) {
        goto cleanup;
    }
    
    // Increase volume for playback
    set_volume(0.7f);
    
    printf("Processing and playing back recording...\n");
    process_audio(capture_buffer, BUFFER_SIZE, 1.2f);
    
    // Prepare playback
    snd_pcm_prepare(playback_handle);
    
    if ((err = write_pcm(playback_handle, capture_buffer, RATE * SECONDS)) < 0) {
        goto cleanup;
    }
    
    // Wait for final playback to complete
    snd_pcm_drain(playback_handle);

cleanup:
    free(buffer);
    free(capture_buffer);
    snd_pcm_close(playback_handle);
    snd_pcm_close(capture_handle);
    
    return err < 0 ? 1 : 0;
}
