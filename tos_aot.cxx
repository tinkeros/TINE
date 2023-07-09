#include "tos_aot.hxx"
#include "alloc.hxx"
#include "dbg.hxx"
#include "ffi.h"
#include "mem.hxx"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <filesystem>
#include <string_view>
namespace fs = std::filesystem;
#include <ios>
#include <iostream>
using std::ios;
#include <fstream>
#include <string>
#include <utility>

MapCHashVec TOSLoader;

// This code is mostly copied from TempleOS
// and does not look very C++-y
static void LoadOneImport(char** src_, char* mod_base) {
  char *src = *src_, *st_ptr, *ptr = nullptr;
  uintptr_t i = 0;
  uint8_t etype;
  bool first = true;
  while ((etype = *src++)) {
    ptr = mod_base + *(uint32_t*)src;
    src += 4;
    st_ptr = src;
    src += strlen(st_ptr) + 1;
    // First occurance of a string means
    // "repeat this until another name is found"
    if (*st_ptr) {
      if (!first) {
        *src_ = st_ptr - 5;
        return;
      } else {
        first = false;
        if (TOSLoader.find(st_ptr) == TOSLoader.end()) {
          std::cerr << "Unresolved reference " << st_ptr << std::endl;
          CHash tmpiss;
          tmpiss.type = HTT_IMPORT_SYS_SYM;
          tmpiss.mod_header_entry = st_ptr - 5;
          tmpiss.mod_base = mod_base;
          TOSLoader[st_ptr].emplace_back(tmpiss);
        } else {
          auto& v = TOSLoader[st_ptr];
          for (auto& tmp : v) {
            if (tmp.type == HTT_IMPORT_SYS_SYM)
              continue;
            i = (uintptr_t)tmp.val;
            break;
          }
        }
      }
    }
    // probably breaks strict aliasing :(
    switch (etype) {
    case IET_REL_I8:
      *(int8_t*)ptr = (char*)i - ptr - 1;
      break;
    case IET_IMM_U8:
      *(uint8_t*)ptr = i;
      break;
    case IET_REL_I16:
      *(int16_t*)ptr = (char*)i - ptr - 2;
      break;
    case IET_IMM_U16:
      *(int16_t*)ptr = i;
      break;
    case IET_REL_I32:
      *(uint32_t*)ptr = (char*)i - ptr - 4;
      break;
    case IET_IMM_U32:
      *(uint32_t*)ptr = i;
      break;
    case IET_REL_I64:
      *(int64_t*)ptr = (char*)i - ptr - 8;
      break;
    case IET_IMM_I64:
      *(int64_t*)ptr = static_cast<int64_t>(i);
      break;
    }
  }
  *src_ = src - 1;
}

static void SysSymImportsResolve(char* st_ptr) {
  char* ptr;
  if (TOSLoader.find(st_ptr) == TOSLoader.end())
    return;
  auto& v = TOSLoader[st_ptr];
  for (auto& sym : v) {
    if (sym.type != HTT_IMPORT_SYS_SYM)
      continue;
    ptr = sym.mod_header_entry;
    LoadOneImport(&ptr, sym.mod_base);
    sym.type = HTT_INVALID;
  }
}

static void LoadPass1(char* src, char* mod_base) {
  char *ptr, *st_ptr;
  uintptr_t i;
  size_t cnt;
  uint8_t etype;
  CHash tmpex;
  while ((etype = *src++)) {
    i = *(uint32_t*)src;
    src += 4;
    st_ptr = src;
    src += strlen(st_ptr) + 1;
    switch (etype) {
    case IET_REL32_EXPORT:
    case IET_IMM32_EXPORT:
    case IET_REL64_EXPORT:
    case IET_IMM64_EXPORT:
      tmpex.type = HTT_EXPORT_SYS_SYM;
      if (etype == IET_IMM32_EXPORT || etype == IET_IMM64_EXPORT)
        tmpex.val = (void*)i;
      else
        tmpex.val = mod_base + i;
      TOSLoader[st_ptr].emplace_back(tmpex);
      SysSymImportsResolve(st_ptr);
      break;
    case IET_REL_I0 ... IET_IMM_I64:
      src = st_ptr - 5;
      LoadOneImport(&src, mod_base);
      break;
    // 32bit addrs
    case IET_ABS_ADDR: {
      cnt = i;
      for (size_t j = 0; j < cnt; j++) {
        ptr = mod_base + *(uint32_t*)src;
        src += 4;
        *(uint32_t*)ptr += (uintptr_t)mod_base;
      }
    } break;
    default:; // the other ones wont be used
              // so im not implementing them
    }
  }
}

static void LoadPass2(char* src, char* mod_base) {
  char* st_ptr;
  uint32_t i;
  uint8_t etype;
  while ((etype = *src++)) {
    i = *(uint32_t*)src;
    src += 4;
    st_ptr = src;
    src += strlen(st_ptr) + 1;
    switch (etype) {
    case IET_MAIN:
      FFI_CALL_TOS_0_ZERO_BP(mod_base + i);
      break;
    case IET_ABS_ADDR:
      src += sizeof(uint32_t) * i;
      break;
    case IET_CODE_HEAP:
    case IET_ZEROED_CODE_HEAP:
      src += 4 + sizeof(uint32_t) * i;
      break;
    case IET_DATA_HEAP:
    case IET_ZEROED_DATA_HEAP:
      src += 8 + sizeof(uint32_t) * i;
      break;
    }
  }
}

extern "C" struct __attribute__((packed)) CBinFile {
  uint16_t jmp;
  uint8_t mod_align_bits, pad;
  union {
    char bin_signature[4];
    uint32_t sig;
  };
  int64_t org, patch_table_offset, file_size;
  char data[]; // FAMs are technically illegal in
               // standard c++ but whatever
};

void LoadHCRT(std::string const& name) {
  std::ifstream f{name, ios::in | ios::binary};
  if (!f) {
    std::cerr << "CANNOT FIND TEMPLEOS BINARY FILE " << name << std::endl;
    std::terminate();
  }
  char* bfh_addr;
  auto sz = fs::file_size(name);
  f.read(bfh_addr = VirtAlloc<char>(sz), sz);
  // I think this breaks strict aliasing but
  // I dont think it matters because its packed(?)
  auto bfh = reinterpret_cast<CBinFile*>(bfh_addr);
  if (std::string_view{bfh->bin_signature, 4} != "TOSB") {
    std::cerr << "INVALID TEMPLEOS BINARY FILE " << name << std::endl;
    std::terminate();
  }
  LoadPass1(bfh_addr + bfh->patch_table_offset, bfh->data);
#ifndef _WIN32
  signal(SIGUSR2, (void (*)(int))TOSLoader["__InterruptCoreRoutine"][0].val);
#endif
  LoadPass2(bfh_addr + bfh->patch_table_offset, bfh->data);
}

void BackTrace() {
  static size_t sz = 0;
  std::string last;
  static std::vector<std::string> sorted;
  static bool init = false;
  if (!init) {
    for (auto const& e : TOSLoader) {
      auto const& [name, v] = e;
      sorted.emplace_back(name);
    }
    sz = sorted.size();
    std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b) {
      return TOSLoader[a][0].val < TOSLoader[b][0].val;
    });
    init = true;
  }
  auto rbp = (void**)__builtin_frame_address(0);
  void* oldp;
  void* ptr = __builtin_return_address(1);
  while (rbp) {
    oldp = nullptr;
    last = "UNKOWN";
    size_t idx;
    for (idx = 0; idx < sz; idx++) {
      void* curp = TOSLoader[sorted[idx]][0].val;
      if (curp == ptr) {
        std::cerr << sorted[idx] << std::endl;
      } else if (curp > ptr) {
        std::cerr << last << "[" << ptr << "+"
                  << reinterpret_cast<void*>((char*)ptr - (char*)oldp) << "]\n";
        goto next;
      }
      oldp = curp;
      last = sorted[idx];
    }
  next:;
    ptr = rbp[1];
    rbp = (void**)*rbp;
  }
  std::cerr << std::endl;
}

// who the fuck cares about memory leaks
// its gonna be executed once or twice in
// the entire debug session not to mention
// this function wont even be called in normal
// circumstances
#define str_dup(s) (strcpy(new char[strlen(s) + 1], s))

// great when you use lldb and get a fault
// (lldb) p (char*)WhichFun($pc)
__attribute__((used)) char* WhichFun(void* ptr) {
  size_t sz = TOSLoader.size();
  std::string last;
  static std::vector<std::string> sorted;
  static bool init = false;
  if (!init) {
    for (auto const& e : TOSLoader) {
      auto const& [name, v] = e;
      sorted.emplace_back(name);
    }
    std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b) {
      return TOSLoader[a][0].val < TOSLoader[b][0].val;
    });
    init = true;
  }
  for (size_t idx = 0; idx < sz; idx++) {
    void* curp = TOSLoader[sorted[idx]][0].val;
    if (curp == ptr) {
      std::cerr << sorted[idx] << std::endl;
    } else if (curp > ptr) {
      return str_dup(last.c_str());
    }
    last = sorted[idx];
  }
  return str_dup(last.c_str());
}
