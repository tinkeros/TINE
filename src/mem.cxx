#include "mem.hxx"
#include "alloc.hxx"

#include <fstream>
#include <string>

using std::ios;

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>

#ifndef _WIN32
#include <sys/mman.h>
#else
// clang-format off
#include <windows.h>
#include <memoryapi.h>
// clang-format on
#endif

#ifdef __linux__
static inline uint64_t Hex2U64(char const* ptr, char const** res) {
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
  size_t const pad = sz % page_size == 0 ? 0 : page_size;
  // sz / page_size * page_size seems meaningless
  // but its actually aligning sz to the page size
  size_t const padded_sz = sz / page_size * page_size + pad;
  void* ret;
  if (low32) { // code heap
    // MAP_32BIT is actually 31 bits(which is actually lucky for us)
    ret = mmap(nullptr, padded_sz, PROT_EXEC | PROT_WRITE | PROT_READ,
               MAP_PRIVATE | MAP_ANON | MAP_32BIT, -1, 0);
#ifdef __linux__
    // I hear that linux doesn't like addresses within the first 16bits
    if (ret == MAP_FAILED) {
      uintptr_t down = 0x10000;
      std::ifstream map{"/proc/self/maps", ios::binary | ios::in};
      std::string buffer;
      // just fs::file_size() wont work lmao
      while (std::getline(map, buffer)) {
        char const* ptr = buffer.data();
        uint64_t lower = Hex2U64(ptr, &ptr);
        // MAP_FIXED wants us to align `down` to the page size
        auto const pag = down % page_size;
        if (pag > 0)
          down += page_size - pag;
        // basically finds a gap between the previous line's upper address
        // and the current line's lower address so it can allocate there
        if (lower - down >= padded_sz && lower > down)
          goto found;
        // ignore '-'
        // cat /proc/self/maps for an explanation
        ++ptr;
        uint64_t upper = Hex2U64(ptr, &ptr);
        down = upper;
      }
    found:
      if (down > MAX_CODE_HEAP_ADDR)
        return nullptr;
      ret = mmap(reinterpret_cast<void*>(down), padded_sz,
                 PROT_EXEC | PROT_WRITE | PROT_READ,
                 MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
    } else
      return ret;
#endif
  } else // data heap
    ret = mmap(nullptr, padded_sz, PROT_WRITE | PROT_READ,
               MAP_PRIVATE | MAP_ANON, -1, 0);
  if (ret == MAP_FAILED)
    return nullptr;
  return ret;
#else
  if (low32) { // code heap
    extern DWORD dwAllocationGranularity;
    // https://archive.md/ugIUC
    MEMORY_BASIC_INFORMATION mbi{};
    // we initialize alloc with the granularity because NULL
    // will fail with VirtualQuery so we need to start
    // from a reasonable small value
    uintptr_t alloc = dwAllocationGranularity, addr;
    while (alloc <= MAX_CODE_HEAP_ADDR) {
      if (0 == VirtualQuery(reinterpret_cast<void*>(alloc), &mbi, sizeof mbi))
        return nullptr;
      alloc = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
      // clang-format off
      //
      // Fancy code to align because VirtualQuery might not give us an aligned base addr
      // and VirtualAlloc likes values to be aligned to dwAllocationGranularity
      // lets say we want to align to 0b100(an arbitrary dwAllocationGranularity)
      //   0b1001
      // + 0b0011 <- (0b100 - 1), flips bottom bits
      // ----------
      //   0b1110
      // & 0b1100 <- ~(0b100 - 1), flipped
      // ----------
      //   0b1100 <- rounded up aligned value
      //
      // It'll be the same if it's already aligned
      //
      // We round up to get the next free allocation granularity unit
      // since VirtualAlloc() will round down the address to the nearest
      // granularity unit multiple and mbi.BaseAddress set by VirtualQuery()
      // is aligned to the page boundary which can interlap with an occupied
      // allocation granularity unit
      //
      // clang-format on
      addr = ((uintptr_t)mbi.BaseAddress + dwAllocationGranularity - 1) &
             ~(dwAllocationGranularity - 1);
      if (mbi.State & MEM_FREE && sz <= alloc - addr)
        return VirtualAlloc(reinterpret_cast<void*>(addr), sz,
                            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    return nullptr;
  } else // data heap
    return VirtualAlloc(nullptr, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    // VirtualAlloc returns null on error
#endif
}

void FreeVirtualChunk(void* ptr, [[maybe_unused]] size_t sz) {
#ifdef _WIN32
  VirtualFree(ptr, 0, MEM_RELEASE);
#else
  size_t const pad = sz % page_size == 0 ? 0 : page_size;
  size_t const padded_sz = sz / page_size * page_size + pad;
  munmap(ptr, padded_sz);
#endif
}

// vim: set expandtab ts=2 sw=2 :
