#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>

#define PCM_DEVICE "default"
#define MIXER_NAME "default"

int setup_mixer_controls(snd_mixer_t *mixer) {
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;
    
    // Allocate mixer simple element identifier
    snd_mixer_selem_id_alloca(&sid);
    
    // Setup Volume Control
    snd_mixer_selem_id_set_name(sid, "Master");
    elem = snd_mixer_find_selem(mixer, sid);
    if (elem) {
        long min, max, value;
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        // Set volume to 80%
        value = min + (max - min) * 0.8;
        snd_mixer_selem_set_playback_volume_all(elem, value);
    }
    
    // Setup Capture Control
    snd_mixer_selem_id_set_name(sid, "Capture");
    elem = snd_mixer_find_selem(mixer, sid);
    if (elem) {
        // Enable capture
        snd_mixer_selem_set_capture_switch_all(elem, 1);
    }
    
    return 0;
}

int setup_pcm_playback() {
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    unsigned int rate = 44100;
    int channels = 2;
    
    // Open PCM device
    if (snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        printf("Error opening PCM device\n");
        return -1;
    }
    
    // Allocate parameters object
    snd_pcm_hw_params_alloca(&params);
    
    // Fill with default values
    snd_pcm_hw_params_any(pcm_handle, params);
    
    // Set parameters
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handle, params, channels);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0);
    
    // Write parameters
    if (snd_pcm_hw_params(pcm_handle, params) < 0) {
        printf("Error setting PCM parameters\n");
        return -1;
    }
    
    return 0;
}

int main() {
    snd_mixer_t *mixer;
    int err;
    
    // Open mixer
    if ((err = snd_mixer_open(&mixer, 0)) < 0) {
        printf("Mixer open error: %s\n", snd_strerror(err));
        return -1;
    }
    
    // Attach to default device
    if ((err = snd_mixer_attach(mixer, MIXER_NAME)) < 0) {
        printf("Mixer attach error: %s\n", snd_strerror(err));
        snd_mixer_close(mixer);
        return -1;
    }
    
    // Register mixer
    if ((err = snd_mixer_selem_register(mixer, NULL, NULL)) < 0) {
        printf("Mixer register error: %s\n", snd_strerror(err));
        snd_mixer_close(mixer);
        return -1;
    }
    
    // Load mixer elements
    if ((err = snd_mixer_load(mixer)) < 0) {
        printf("Mixer load error: %s\n", snd_strerror(err));
        snd_mixer_close(mixer);
        return -1;
    }
    
    // Setup mixer controls
    setup_mixer_controls(mixer);
    
    // Setup PCM playback
    setup_pcm_playback();
    
    // Exception handling example
    int exception_status = 0;
    char *error_message = NULL;
    
    // Channel mapping example
    snd_pcm_t *capture_handle;
    if (snd_pcm_open(&capture_handle, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0) >= 0) {
        snd_pcm_hw_params_t *cap_params;
        snd_pcm_hw_params_alloca(&cap_params);
        snd_pcm_hw_params_any(capture_handle, cap_params);
        
        // Set channel map (example: stereo)
        snd_pcm_chmap_t *chmap = malloc(sizeof(snd_pcm_chmap_t) + 2 * sizeof(unsigned int));
        chmap->channels = 2;
        chmap->pos[0] = SND_CHMAP_FL;  // Front Left
        chmap->pos[1] = SND_CHMAP_FR;  // Front Right
        
        snd_pcm_set_chmap(capture_handle, chmap);
        free(chmap);
        snd_pcm_close(capture_handle);
    }
    
    // Clean up
    snd_mixer_close(mixer);
    return 0;
}
