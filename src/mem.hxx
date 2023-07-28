#pragma once

#include <stddef.h>
#include <stdint.h>

void* NewVirtualChunk(size_t sz, bool low32);
void  FreeVirtualChunk(void* ptr, size_t s);

// from Glossary.DD
/*
 * TempleOS uses the asm CALL inst, exclusively, and that inst is limited to
 * calling routines +/-2Gig from the current code location. To prevent
 * out-of-range issues, I decided to separate code and data, placing all code
 * within the lowest 2Gig of memory, addresses 00000000-7FFFFFFF. The compiler
 * and Load()er alloc memory from the code heap to store code and glbl vars,
 * unless the compiler option "OPTf_GLBLS_ON_DATA_HEAP" is used. When programs
 * call MAlloc() is from the data heap, which in not limited in size, except by
 * physical RAM memory. You can alloc from any heap in any task at any time on
 * any core, even making independent (with MemPagAlloc) heaps.
 */
enum : uint32_t {
  MAX_CODE_HEAP_ADDR = 0x7fffFFFF,
};

// vim: set expandtab ts=2 sw=2 :
