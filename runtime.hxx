#pragma once

#include <stddef.h>
#include <stdint.h>

void* HolyMAlloc(size_t sz);
void* HolyCAlloc(size_t sz);
void HolyFree(void* p);
char* HolyStrDup(char const* s);
void RegisterFuncPtrs();
uint64_t mp_cnt(void*);
