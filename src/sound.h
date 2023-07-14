#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

void InitSound(void);
void SndFreq(uint64_t);
double GetVolume(void);
void SetVolume(double);

#ifdef __cplusplus
}
#endif

// vim: set expandtab ts=2 sw=2 :
