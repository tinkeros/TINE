#pragma once

#include <stddef.h>
#include <stdint.h>

// Technically UB by the standard but... meh
#ifndef _WIN32
  #define __stdcall
#endif

uint64_t GetTicks();

void* GetFs();
void SetFs(void*);

void* GetGs();
void SetGs(void*);

size_t CoreNum();

void InterruptCore(size_t core);

// https://archive.md/nKvoK
typedef void* __stdcall ThreadCallback(void*);
void LaunchCore0(ThreadCallback* fp);
void WaitForCore0();

void CreateCore(size_t core, void* fp);
void ShutdownCore(size_t core);
void ShutdownCores(int ec);

void AwakeFromSleeping(size_t core);

void SleepHP(uint64_t us);

// vim: set expandtab ts=2 sw=2 :
