#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>

#define RATE 44100
#define CHANNELS 2
#define SECONDS 5
#define SAMPLE_SIZE (sizeof(short))
#define FRAME_SIZE (CHANNELS * SAMPLE_SIZE)
#define BUFFER_SIZE (RATE * SECONDS * FRAME_SIZE)
#define PERIOD_SIZE 4096

// Previous functions remain the same until main()...
[Previous functions remain exactly the same]

int main() {
    snd_pcm_t *playback_handle, *capture_handle;
    int err;
    short *noise_buffer, *capture_buffer;

    // Allocate buffers
    noise_buffer = (short *)malloc(BUFFER_SIZE);
    capture_buffer = (short *)malloc(BUFFER_SIZE);
    if (!noise_buffer || !capture_buffer) {
        fprintf(stderr, "Buffer allocation failed\n");
        return 1;
    }
    memset(capture_buffer, 0, BUFFER_SIZE);

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

    // Initial volume
    set_volume(0.5f);

    // Step 1: Generate and play noise
    printf("Generating noise...\n");
    generate_noise(noise_buffer, BUFFER_SIZE, 0.1f);
    
    printf("Playing noise...\n");
    snd_pcm_prepare(playback_handle);
    if ((err = write_pcm(playback_handle, noise_buffer, RATE * SECONDS)) < 0) {
        goto cleanup;
    }
    
    // Ensure playback is complete
    snd_pcm_drain(playback_handle);
    
    // Small delay to ensure complete drain
    usleep(100000);  // 100ms delay

    // Step 2: Record
    printf("Starting recording...\n");
    // Drop any old data
    snd_pcm_drop(capture_handle);
    // Prepare for new capture
    snd_pcm_prepare(capture_handle);
    
    // Start the actual recording
    if ((err = read_pcm(capture_handle, capture_buffer, RATE * SECONDS)) < 0) {
        fprintf(stderr, "Recording failed: %s\n", snd_strerror(err));
        goto cleanup;
    }
    
    printf("Recording complete.\n");
    
    // Ensure capture is complete
    snd_pcm_drain(capture_handle);
    
    // Step 3: Playback the recording
    printf("Playing back recording...\n");
    set_volume(0.7f);
    process_audio(capture_buffer, BUFFER_SIZE, 1.2f);
    
    // Prepare for playback
    snd_pcm_drop(playback_handle);
    snd_pcm_prepare(playback_handle);
    
    if ((err = write_pcm(playback_handle, capture_buffer, RATE * SECONDS)) < 0) {
        goto cleanup;
    }
    
    // Wait for final playback to complete
    snd_pcm_drain(playback_handle);
    printf("Playback complete.\n");

cleanup:
    free(noise_buffer);
    free(capture_buffer);
    snd_pcm_close(playback_handle);
    snd_pcm_close(capture_handle);
    
    return err < 0 ? 1 : 0;
}
