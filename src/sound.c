#include <SDL2/SDL.h>

#include <math.h>
#include <stdint.h>

#include "sound.h"

static SDL_AudioDeviceID output;
static uint64_t sample, freq;
static SDL_AudioSpec have;
static double vol = .1;

static void AudioCB(void* userdata, Uint8* out, int len) {
  (void)userdata;
  for (int i = 0; i < len / have.channels; ++i) {
    double t = (double)++sample / have.freq;
    double amp = -1.0 + 2.0 * roundf(fmod(2.0 * t * freq, 1.0));
    Sint8 maxed = (amp > 0) ? 127 : -127;
    maxed *= vol;
    if (!freq)
      maxed = 0;
    for (Uint8 j = 0; j < have.channels; ++j)
      out[have.channels * i + j] = maxed;
  }
}

void InitSound(void) {
  if (!SDL_WasInit(SDL_INIT_AUDIO))
    SDL_Init(SDL_INIT_AUDIO);
  SDL_AudioSpec want = {
      .freq = 24000,
      .format = AUDIO_S8,
      .channels = 2,
      .samples = 256,
      .callback = AudioCB,
  };
  output = SDL_OpenAudioDevice(NULL, 0, &want, &have,
                               SDL_AUDIO_ALLOW_FREQUENCY_CHANGE |
                                   SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
  SDL_PauseAudioDevice(output, 0);
}

void SndFreq(uint64_t f) {
  freq = f;
}

void SetVolume(double v) {
  vol = v;
}

double GetVolume(void) {
  return vol;
}

// vim: set expandtab ts=2 sw=2 :
