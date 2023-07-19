#include "mem.hxx"
#include "main.hxx"

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
extern DWORD dwAllocationGranularity;
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
  // explanation of (x+y-1)&~(y-1) on the bottom windows code
  // page_size is a power of 2 so this works
  size_t const padded_sz = (sz + page_size - 1) & ~(page_size - 1);
  void* ret;
  if (low32) { // code heap
    // MAP_32BIT is actually MAP_31BIT(which is actually lucky for us)
    ret = mmap(nullptr, padded_sz, PROT_EXEC | PROT_WRITE | PROT_READ,
               MAP_PRIVATE | MAP_ANON | MAP_32BIT, -1, 0);
#ifdef __linux__
    if (ret == MAP_FAILED) {
      // side note: Linux doesn't seem to like allocating stuff below 31 bits
      // (<0x40000000). I don't know why so technically we have 1GB less space
      // for the code heap than on Windows or maybe FreeBSD(I don't have it
      // installed) but it won't really matter since machine code doesn't take
      // up a lot of space
      uintptr_t down = 0;
      std::ifstream map{"/proc/self/maps", ios::binary | ios::in};
      std::string buffer;
      // just fs::file_size() wont work lmao
      while (std::getline(map, buffer)) {
        char const* ptr = buffer.data();
        uint64_t lower = Hex2U64(ptr, &ptr);
        // MAP_FIXED wants us to align `down` to the page size
        down = (down + page_size - 1) & ~(page_size - 1);
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
    // https://archive.md/ugIUC
    MEMORY_BASIC_INFORMATION mbi{};
    // we initialize alloc with the granularity because NULL
    // will fail with VirtualQuery so we need to start
    // from a reasonable small value
    uintptr_t alloc = dwAllocationGranularity, addr;
    while (alloc <= MAX_CODE_HEAP_ADDR) {
      if (0 == VirtualQuery(reinterpret_cast<void*>(alloc), &mbi, sizeof mbi))
        return nullptr;
      alloc = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
      // clang-format off
      //
      // Fancy code to align to round up to the nearest allocation granularity unit
      // since VirtualAlloc() will round down the address to the nearest
      // granularity unit multiple and mbi.BaseAddress set by VirtualQuery()
      // is aligned to the page boundary which can interlap with an occupied
      // allocation granularity unit
      //
      // This works because dwAllocationGranularity is a power of 2
      // (so it has only 1 bit that's 1, eg 0b10000)
      //
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
      // clang-format on
      addr = (reinterpret_cast<uintptr_t>(mbi.BaseAddress) +
              dwAllocationGranularity - 1) &
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
  size_t const padded_sz = (sz + page_size - 1) & ~(page_size - 1);
  munmap(ptr, padded_sz);
#endif
}

// vim: set expandtab ts=2 sw=2 :
