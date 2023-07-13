#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
// please policeman is it a test?
#include <stddef.h>

uint64_t FFI_CALL_TOS_0(void*);
uint64_t FFI_CALL_TOS_1(void*, uint64_t);
uint64_t FFI_CALL_TOS_2(void*, uint64_t, uint64_t);
uint64_t FFI_CALL_TOS_3(void*, uint64_t, uint64_t, uint64_t);
uint64_t FFI_CALL_TOS_4(void*, uint64_t, uint64_t, uint64_t, uint64_t);
uint64_t FFI_CALL_TOS_5(void*, uint64_t, uint64_t, uint64_t, uint64_t,
                        uint64_t);
uint64_t FFI_CALL_TOS_6(void*, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                        uint64_t);
uint64_t FFI_CALL_TOS_7(void*, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                        uint64_t, uint64_t);
uint64_t FFI_CALL_TOS_8(void*, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                        uint64_t, uint64_t, uint64_t);
uint64_t FFI_CALL_TOS_0_ZERO_BP(void*);

#ifdef __cplusplus
}
#endif

// vim: set expandtab ts=2 sw=2 :
