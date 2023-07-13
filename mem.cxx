#include "mem.hxx"

#include <fstream>
#include <ios>
using std::ios;
#include <memory>
#include <string>
#include <utility>

#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>
#else
// clang-format off
#include <windows.h>
#include <winnt.h>
#include <memoryapi.h>
#include <sysinfoapi.h>
// clang-format on
#endif

#ifdef __linux__
static inline uint64_t Hex2U64(char const* __restrict ptr,
                               char const** __restrict res) {
  uint64_t ret = 0;
  char c;
  while (isxdigit(c = *ptr)) {
    ret <<= 4;
    ret |= isalpha(c) ? toupper(c) - 'A' + 10 : c - '0';
    ++ptr;
  }
  *res = ptr;
  return ret;
}
#endif

void* NewVirtualChunk(size_t sz, bool low32) {
#ifndef _WIN32
  static size_t ps = 0;
  if (!ps)
    ps = sysconf(_SC_PAGESIZE);
  size_t pad = ps;
  void* ret;
  pad = sz % ps;
  if (pad)
    pad = ps;
  if (low32) { // code heap
    // sz / ps * ps seems meaningless
    // but its actually aligning sz to ps(page size)
    // MAP_32BIT is actually 31 bits(which is actually lucky for us)
    ret = mmap(nullptr, sz / ps * ps + pad, PROT_EXEC | PROT_WRITE | PROT_READ,
               MAP_PRIVATE | MAP_ANON | MAP_32BIT, -1, 0);
#ifdef __linux__
    // I hear that linux doesn't like addresses within the first 16bits
    if (ret == MAP_FAILED) {
      uintptr_t down = 0xffff;
      std::ifstream map{"/proc/self/maps", ios::binary | ios::in};
      std::string buffer;
      // just fs::file_size() wont work lmao
      while (std::getline(map, buffer)) {
        char const* ptr = buffer.data();
        uint64_t lower = Hex2U64(ptr, &ptr);
        // basically finds a gap between the previous line's upper address
        // and the current line's lower address so it can allocate there
        if (lower - down >= sz / ps * ps + pad && lower > down) {
          goto found;
        }
        // ignore '-'
        ++ptr;
        uint64_t upper = Hex2U64(ptr, &ptr);
        down = upper;
      }
    found:
      if (down > MAX_CODE_HEAP_ADDR)
        return nullptr;
      ret = mmap(reinterpret_cast<void*>(down), sz / ps * ps + pad,
                 PROT_EXEC | PROT_WRITE | PROT_READ,
                 MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
    } else
      return ret;
#endif
  } else // data heap
    ret = mmap(nullptr, sz / ps * ps + pad, PROT_WRITE | PROT_READ,
               MAP_PRIVATE | MAP_ANON, -1, 0);
  if (ret == MAP_FAILED)
    return nullptr;
  return ret;
#else
  if (low32) { // code heap
    // https://archive.md/ugIUC
    static DWORD dwAllocationGranularity = 0;
    if (dwAllocationGranularity == 0) {
      SYSTEM_INFO si;
      GetSystemInfo(&si);
      dwAllocationGranularity = si.dwAllocationGranularity;
    }
    MEMORY_BASIC_INFORMATION ent{};
    uint64_t alloc = dwAllocationGranularity, addr;
    while (alloc <= MAX_CODE_HEAP_ADDR) {
      if (!VirtualQuery((void*)alloc, &ent, sizeof ent))
        return nullptr;
      alloc = (uint64_t)ent.BaseAddress + ent.RegionSize;
      // Fancy code to round up because
      // address is rounded down with
      // VirtualAlloc
      addr = ((uint64_t)ent.BaseAddress + dwAllocationGranularity - 1) &
             ~(dwAllocationGranularity - 1);
      if (ent.State == MEM_FREE && sz <= alloc - addr)
        return VirtualAlloc((void*)addr, sz, MEM_COMMIT | MEM_RESERVE,
                            PAGE_EXECUTE_READWRITE);
    }
    return nullptr;
  } else // data heap
    return VirtualAlloc(nullptr, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif
}

void FreeVirtualChunk(void* ptr, [[maybe_unused]] size_t s) {
#ifdef _WIN32
  VirtualFree(ptr, 0, MEM_RELEASE);
#else
  static size_t ps = 0;
  if (!ps)
    ps = sysconf(_SC_PAGESIZE);
  int64_t pad;
  pad = s % ps;
  if (pad)
    pad = ps;
  munmap(ptr, s / ps * ps + pad);
#endif
}

// vim: set expandtab ts=2 sw=2 :
