#include "runtime.hxx"
#include "TOSPrint.hxx"
#include "alloc.hxx"
#include "ffi.h"
#include "main.hxx"
#include "mem.hxx"
#include "multic.hxx"
#include "sdl_window.hxx"
#include "sound.h"
#include "tos_aot.hxx"
#include "vfs.hxx"

#include <ios>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
using std::ios;
#include <filesystem>
#include <fstream>
#include <memory>
namespace fs = std::filesystem;
#include <thread>
using std::thread;

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <winbase.h>
#include <fileapi.h>
#include <memoryapi.h>
#include <shlwapi.h>
// clang-format on
#else
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "dyad.h"
#include "linenoise-ng/linenoise.h"

void HolyFree(void* ptr) {
  static void* fptr = nullptr;
  if (!fptr)
    fptr = TOSLoader["_FREE"][0].val;
  FFI_CALL_TOS_1(fptr, (uintptr_t)ptr);
}

void* HolyMAlloc(size_t sz) {
  static void* fptr = nullptr;
  if (!fptr)
    fptr = TOSLoader["_MALLOC"][0].val;
  return (void*)FFI_CALL_TOS_2(fptr, sz, 0 /*NULL*/);
}

void* HolyCAlloc(size_t sz) {
  auto ret = HolyAlloc<uint8_t>(sz);
  std::fill(ret, ret + sz, 0);
  return ret;
}

char* HolyStrDup(char const* str) {
  return strcpy(HolyAlloc<char>(strlen(str) + 1), str);
}

static FILE* VFsFOpen(char const* path, char const* m) {
  std::string p = VFsFileNameAbs(path);
  return fopen(p.c_str(), m);
}

static void STK_DyadInit() {
  static bool init = false;
  if (init)
    return;
  init = true;
  dyad_init();
  dyad_setUpdateTimeout(0.);
}

static void STK_DyadUpdate() {
  dyad_update();
}

static void STK_DyadShutdown() {
  dyad_shutdown();
}

static void* STK_DyadNewStream() {
  return dyad_newStream();
}

static int64_t STK_DyadListen(intptr_t* stk) {
  return dyad_listen((dyad_Stream*)stk[0], static_cast<int>(stk[1]));
}

static int64_t STK_DyadConnect(intptr_t* stk) {
  return dyad_connect((dyad_Stream*)stk[0], (char const*)stk[1],
                      static_cast<int>(stk[2]));
}

static void STK_DyadWrite(intptr_t* stk) {
  dyad_write((dyad_Stream*)stk[0], (void*)stk[1], static_cast<int>(stk[2]));
}

static void STK_DyadEnd(uintptr_t* stk) {
  dyad_end((dyad_Stream*)stk[0]);
}

static void STK_DyadClose(uintptr_t* stk) {
  dyad_close((dyad_Stream*)stk[0]);
}

static char* STK_DyadGetAddress(uintptr_t* stk) {
  const char* ret = dyad_getAddress((dyad_Stream*)stk[0]);
  return HolyStrDup(ret);
}

static void DyadReadCB(dyad_Event* e) {
  FFI_CALL_TOS_4(e->udata, (uintptr_t)e->stream, (uintptr_t)e->data, e->size,
                 (uintptr_t)e->udata2);
}

static void STK_DyadSetReadCallback(uintptr_t* stk) {
  // This is for a line of text
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_LINE, DyadReadCB,
                   (void*)stk[1], (void*)stk[2]);
}

static void STK_DyadSetDataCallback(uintptr_t* stk) {
  // This is for binary data
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_DATA, DyadReadCB,
                   (void*)stk[1], (void*)stk[2]);
}

static void DyadListenCB(dyad_Event* e) {
  FFI_CALL_TOS_2(e->udata, (uintptr_t)e->remote, (uintptr_t)e->udata2);
}

static void DyadCloseCB(dyad_Event* e) {
  FFI_CALL_TOS_2(e->udata, (uintptr_t)e->stream, (uintptr_t)e->udata2);
}

static void STK_DyadSetOnCloseCallback(uintptr_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_CLOSE, DyadCloseCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetOnConnectCallback(uintptr_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_CONNECT, DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetOnDestroyCallback(uintptr_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_DESTROY, DyadCloseCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetOnErrorCallback(uintptr_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_ERROR, DyadCloseCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetOnReadyCallback(uintptr_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_READY, DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetOnTickCallback(uintptr_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_TICK, DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetOnTimeoutCallback(uintptr_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_TIMEOUT, DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}

static void STK_DyadSetOnListenCallback(uintptr_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_ACCEPT, DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}

static void STK_DyadSetTimeout(uintptr_t* stk) {
  dyad_setTimeout((dyad_Stream*)stk[0], ((double*)stk)[1]);
}

static void STK_DyadSetNoDelay(intptr_t* stk) {
  dyad_setNoDelay((dyad_Stream*)stk[0], static_cast<int>(stk[1]));
}

static void STK_UnblockSignals() {
#ifndef _WIN32
  sigset_t all;
  sigfillset(&all);
  sigprocmask(SIG_UNBLOCK, &all, nullptr);
#endif
}

static void STK__GrPaletteColorSet(uint64_t* stk) {
  GrPaletteColorSet(stk[0], stk[1]);
}

static uint64_t STK___IsValidPtr(uintptr_t* stk) {
#ifdef _WIN32
  // Wine doesnt like the
  // IsBadReadPtr,so use a polyfill

  // wtf IsBadReadPtr gives me a segfault so i just have to use this
  // polyfill lmfao
  // #ifdef __WINE__
  MEMORY_BASIC_INFORMATION mbi{};
  if (VirtualQuery((void*)stk[0], &mbi, sizeof mbi)) {
    // https://archive.md/ehBq4
    DWORD const mask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                       PAGE_EXECUTE | PAGE_EXECUTE_READ |
                       PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return !!(mbi.Protect & mask);
  }
  return false;
  /*#else
    return !IsBadReadPtr((void*)stk[0], 8);
  #endif*/

#else
  // #ifdef __FreeBSD__
  static size_t ps = 0;
  if (!ps)
    ps = sysconf(_SC_PAGESIZE);
  stk[0] /= ps; // align to
  stk[0] *= ps; // page boundary
  // https://archive.md/Aj0S4
  return -1 != msync((void*)stk[0], ps, MS_ASYNC);
  /*#elif defined(__linux__)
        // TOO FUCKING GODDAMN SLOW!!!!!
    auto constexpr Hex2U64 = [](char const *ptr, char const** res) {
      uint64_t ret = 0;
      char c;
      while (isxdigit(c = *ptr)) {
        ret <<= 4;
        ret |= isalpha(c) ? toupper(c) - 'A' + 10 : c - '0';
        ++ptr;
      }
      *res = ptr;
      return ret;
    };
    std::ifstream map{"/proc/self/maps", ios::binary | ios::in};
    std::string buffer;
    while (std::getline(map, buffer)) {
      char const* ptr = buffer.data();
      uintptr_t lower = Hex2U64(ptr, &ptr);
      ++ptr; // skip '-'
      uintptr_t upper = Hex2U64(ptr, &ptr);
      if (lower <= stk[0] && stk[0] <= upper)
        return true;
    }
    return false;
  #endif*/

#endif
}

static void STK_InterruptCore(uint64_t* stk) {
  InterruptCore(stk[0]);
}

static void STK___BootstrapForeachSymbol(uintptr_t* stk) {
  for (auto& m : TOSLoader) {
    auto& [symname, vec] = m;
    auto& sym = vec[0];
    FFI_CALL_TOS_3((void*)stk[0], (uintptr_t)symname.c_str(),
                   (uintptr_t)sym.val,
                   sym.type == HTT_EXPORT_SYS_SYM ? HTT_FUN : sym.type);
  }
}

static void STK_TOSPrint(uint64_t* stk) {
  TOSPrint((char const*)stk[0], stk[1], (int64_t*)stk + 2);
}

static void STK_DrawWindowUpdate(uintptr_t* stk) {
  DrawWindowUpdate((uint8_t*)stk[0], stk[1]);
}

static uint64_t STK___GetTicksHP(void*) {
#ifndef _WIN32
  struct timespec ts;
  uint64_t theTick = 0U;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  theTick = ts.tv_nsec / 1000;
  theTick += ts.tv_sec * 1000000U;
  return theTick;
#else
  static uint64_t freq = 0;
  uint64_t cur;
  if (!freq) {
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    freq /= 1000000U;
  }
  QueryPerformanceCounter((LARGE_INTEGER*)&cur);
  return cur / freq;
#endif
}

static uint64_t STK___GetTicks(void*) {
  return GetTicks();
}

static void STK_SetKBCallback(uintptr_t* stk) {
  SetKBCallback((void*)stk[0], (void*)stk[1]);
}

static void STK_SetMSCallback(uintptr_t* stk) {
  SetMSCallback((void*)stk[0]);
}

static void STK___AwakeCore(size_t* stk) {
  AwakeFromSleeping(stk[0]);
}

static void STK___SleepHP(uint64_t* stk) {
  SleepHP(stk[0]);
}

static void STK___Sleep(uint64_t* stk) {
  SleepHP(stk[0] * 1000);
}

static void STK_SetFs(uintptr_t* stk) {
  SetFs((void*)stk[0]);
}

static void STK_SetGs(uintptr_t* stk) {
  SetGs((void*)stk[0]);
}

static void STK_SndFreq(uint64_t* stk) {
  SndFreq(stk[0]);
}

static void STK_SetClipboardText(uintptr_t* stk) {
  SetClipboard((char*)stk[0]);
}

static char* STK___GetStr(uintptr_t* stk) {
  char *s = linenoise(reinterpret_cast<char const*>(stk[0])), *r;
  if (s == nullptr)
    return nullptr;
  linenoiseHistoryAdd(s);
  r = HolyStrDup(s);
  free(s);
  return r;
}

static char* STK_GetClipboardText(void*) {
  std::string clip{ClipboardText()};
  return HolyStrDup(clip.c_str());
}

static int64_t STK_FUnixTime(uintptr_t* stk) {
  return VFsUnixTime((char*)stk[0]);
}

static void STK_VFsFTrunc(uintptr_t* stk) {
  fs::resize_file(VFsFileNameAbs((char*)stk[0]), stk[1]);
}

static uint64_t STK___FExists(uintptr_t* stk) {
  return VFsFileExists((char*)stk[0]);
}

#ifndef _WIN32

#include <time.h>

static uint64_t STK_UnixNow(void*) {
  return time(nullptr);
}

#else

static uint64_t STK_UnixNow(void*) {
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  // https://archive.md/xl8qB
  uint64_t time = ft.dwLowDateTime | ((uint64_t)ft.dwHighDateTime << 32),
           adj = 10000 * (uint64_t)11644473600000;
  time -= adj;
  return time / 10000000ull;
}

#endif

size_t mp_cnt(void*) {
  return thread::hardware_concurrency();
}

static void STK___SpawnCore(uintptr_t* stk) {
  CreateCore(stk[0], (void*)stk[1]);
}

static void* STK_NewVirtualChunk(size_t* stk) {
  return NewVirtualChunk(stk[0], stk[1]);
}

static void STK_FreeVirtualChunk(uintptr_t* stk) {
  FreeVirtualChunk((void*)stk[0], stk[1]);
}

static void STK_VFsSetPwd(uintptr_t* stk) {
  VFsSetPwd((char*)stk[0]);
}

static char* STK__BootDrv(void*) {
  return BootDrv();
}

static uint64_t STK_VFsExists(uintptr_t* stk) {
  return VFsFileExists((char*)stk[0]);
}

static uint64_t STK_VFsIsDir(uintptr_t* stk) {
  return VFsIsDir((char*)stk[0]);
}

static int64_t STK_VFsFSize(uintptr_t* stk) {
  return VFsFSize((char*)stk[0]);
}

static uint64_t STK_VFsFRead(uintptr_t* stk) {
  return (uintptr_t)VFsFileRead((char const*)stk[0], (uint64_t*)stk[1]);
}

static uint64_t STK_VFsFWrite(uintptr_t* stk) {
  return VFsFileWrite((char*)stk[0], (char*)stk[1], stk[2]);
}

static uint64_t STK_VFsDirMk(uintptr_t* stk) {
  return VFsDirMk((char*)stk[0], VFS_CDF_MAKE);
}

static uint64_t STK_VFsDir(void*) {
  return (uintptr_t)VFsDir();
}

static uint64_t STK_VFsDel(uintptr_t* stk) {
  return VFsDel((char*)stk[0]);
}

static uint64_t STK_VFsFOpenW(uintptr_t* stk) {
  return (uintptr_t)VFsFOpen((char*)stk[0], "w+b");
}

static uint64_t STK_VFsFOpenR(uintptr_t* stk) {
  return (uintptr_t)VFsFOpen((char*)stk[0], "rb");
}

static void STK_VFsFClose(uintptr_t* stk) {
  fclose((FILE*)stk[0]);
}

static uint64_t STK_VFsFBlkRead(uintptr_t* stk) {
  fflush((FILE*)stk[3]);
  return stk[2] == fread((void*)stk[0], stk[1], stk[2], (FILE*)stk[3]);
}

static uint64_t STK_VFsFBlkWrite(uintptr_t* stk) {
  bool r = stk[2] == fwrite((void*)stk[0], stk[1], stk[2], (FILE*)stk[3]);
  fflush((FILE*)stk[3]);
  return r;
}

static void STK_VFsFSeek(uintptr_t* stk) {
  fseek((FILE*)stk[1], stk[0], SEEK_SET);
}

static void STK_VFsSetDrv(char* stk) {
  VFsSetDrv(stk[0]);
}

static void STK_SetVolume(uint64_t* stk) {
  static_assert(sizeof(double) == sizeof(uint64_t));
  union {
    uint64_t i;
    double flt;
  } un = {stk[0]};
  SetVolume(un.flt);
}

static uint64_t STK_GetVolume(void*) {
  union {
    double flt;
    uint64_t i;
  } un = {GetVolume()};
  return un.i;
}

static void STK_ExitTINE(int64_t* stk) {
  ShutdownTINE(static_cast<int>(stk[0]));
}

// arity must be <= 0xffFF/sizeof U64
static void RegisterFunctionPtr(std::string& blob, char const* name,
                                uintptr_t fp, uint16_t arity) {
  // Function entry point offset from the code blob
  uintptr_t off = blob.size();
  // https://defuse.ca/online-x86-assembler.htm
  // boring register pushing and stack alignment bullshit
  // disas then read the ABIs and Doc/GuideLines.DD if interested
  char const* inst = "\x55\x48\x89\xE5\x48"
                     "\x83\xE4\xF0\x56\x57"
                     "\x41\x52\x41\x53\x41"
                     "\x54\x41\x55\x41\x56"
                     "\x41\x57";
  blob.append(inst, 22);
#ifdef _WIN32
  // https://archive.md/4HDA0#selection-2085.880-2085.1196
  // rcx is the first arg i have to provide in win64 abi
  // last 4 register pushes are for register "home"s
  // that windows wants me to provide
  inst = "\x48\x8D\x4D\x10\x41"
         "\x51\x41\x50\x52\x51";
  blob.append(inst, 10);
#else // sysv
  // rdi is the first arg
  // i have to provide in sysv
  blob.append("\x48\x8D\x7D\x10", 4);
#endif
  // movabs rax, <fp>
  blob.append("\x48\xb8", 2);
  union {
    uintptr_t p;
    char data[8];
  } fu = {fp};
  blob.append(fu.data, 8);
  // call rax
  blob.append("\xFF\xD0", 2);
#ifdef _WIN32
  // can just add to rsp since
  // those 4 registers are volatile
  blob.append("\x48\x83\xC4\x20", 4);
#endif
  // pops stack. boring stuff
  inst = "\x41\x5F\x41\x5E\x41"
         "\x5D\x41\x5C\x41\x5B"
         "\x41\x5A\x5F\x5E";
  blob.append(inst, 14);
  // leave
  blob.push_back('\xC9');
  // clang-format off
  // ret <arity*8>; (8 == sizeof(uint64_t))
  // HolyC ABI is __stdcall, the callee cleans up its own stack
  // unless its variadic so we pop the stack with ret
  //
  // A bit about HolyC ABI: all args are 8 bytes(64 bits)
  // let there be function Foo(I64 i, ...);
  // Foo(2, 4, 5, 6)
  //   argv[2] 6 // RBP + 48
  //   argv[1] 5 // RBP + 40
  //   argv[0] 4 // RBP + 32 <-points- argv(internal var in function)
  //   argc 3(num of varargs) // RBP + 24 <-value- argc(internal var in function)
  //   i  2    // RBP + 16(this is where the stack starts)
  // clang-format on
  // ret
  blob.push_back('\xc2');
  arity *= 8; // sizeof(uint64_t)
  // imm16
  union {
    uint16_t ar;
    char data[2];
  } au = {arity};
  blob.append(au.data, 2);
  CHash sym;
  sym.type = HTT_FUN;
  sym.val = reinterpret_cast<void*>(off);
  TOSLoader[name].emplace_back(sym);
}

void RegisterFuncPtrs() {
  std::string ffi_blob;
#define R_(holy, secular, arity)                                            \
  RegisterFunctionPtr(ffi_blob, holy, reinterpret_cast<uintptr_t>(secular), \
                      arity)
#define S_(name, arity)                \
  RegisterFunctionPtr(ffi_blob, #name, \
                      reinterpret_cast<uintptr_t>(STK_##name), arity)
  R_("__CmdLineBootText", CmdLineBootText, 0);
  R_("__IsCmdLine", IsCmdLine, 0);
  R_("mp_cnt", mp_cnt, 0);
  R_("__CoreNum", CoreNum, 0);
  R_("GetFs", GetFs, 0);
  R_("GetGs", GetGs, 0);
  S_(__IsValidPtr, 1);
  S_(__SpawnCore, 0);
  S_(UnixNow, 0);
  S_(InterruptCore, 1);
  S_(NewVirtualChunk, 2);
  S_(FreeVirtualChunk, 2);
  S_(ExitTINE, 1);
  S_(__GetStr, 1);
  S_(__FExists, 1);
  S_(FUnixTime, 1);
  S_(SetClipboardText, 1);
  S_(GetClipboardText, 0);
  S_(SndFreq, 1);
  S_(__Sleep, 1);
  S_(__SleepHP, 1);
  S_(__AwakeCore, 1);
  S_(SetFs, 1);
  S_(SetGs, 1);
  S_(SetKBCallback, 2);
  S_(SetMSCallback, 1);
  S_(__GetTicks, 0);
  S_(__BootstrapForeachSymbol, 1);
  S_(DrawWindowUpdate, 2);
  S_(UnblockSignals, 0);
  /*
   * In TempleOS variadics, functions follow __cdecl, whereas normally
   * they follow __stdcall which is why the arity argument is needed(RET1 x).
   * Thus we don't have to clean up the stack in variadics.
   */
  S_(TOSPrint, 0);
  S_(DyadInit, 0);
  S_(DyadUpdate, 0);
  S_(DyadShutdown, 0);
  S_(DyadNewStream, 0);
  S_(DyadListen, 2);
  S_(DyadConnect, 3);
  S_(DyadWrite, 3);
  S_(DyadEnd, 1);
  S_(DyadClose, 1);
  S_(DyadGetAddress, 1);
  S_(DyadSetDataCallback, 3);
  S_(DyadSetReadCallback, 3);
  S_(DyadSetOnListenCallback, 3);
  S_(DyadSetOnConnectCallback, 3);
  S_(DyadSetOnCloseCallback, 3);
  S_(DyadSetOnReadyCallback, 3);
  S_(DyadSetOnTimeoutCallback, 3);
  S_(DyadSetOnTickCallback, 3);
  S_(DyadSetOnErrorCallback, 3);
  S_(DyadSetOnDestroyCallback, 3);
  S_(DyadSetTimeout, 2);
  S_(DyadSetNoDelay, 2);
  S_(_BootDrv, 0);
  S_(VFsFTrunc, 2);
  S_(VFsSetPwd, 1);
  S_(VFsExists, 1);
  S_(VFsIsDir, 1);
  S_(VFsFSize, 1);
  S_(VFsFRead, 2);
  S_(VFsFWrite, 3);
  S_(VFsDel, 1);
  S_(VFsDir, 0);
  S_(VFsDirMk, 1);
  S_(VFsFBlkRead, 4);
  S_(VFsFBlkWrite, 4);
  S_(VFsFOpenW, 1);
  S_(VFsFOpenR, 1);
  S_(VFsFClose, 1);
  S_(VFsFSeek, 2);
  S_(VFsSetDrv, 1);
  S_(GetVolume, 0);
  S_(SetVolume, 1);
  S_(__GetTicksHP, 0);
  S_(_GrPaletteColorSet, 2);
  auto blob = VirtAlloc<char>(ffi_blob.size());
  std::copy(ffi_blob.begin(), ffi_blob.end(), blob);
  for (auto& m : TOSLoader) {
    auto& [symname, vec] = m;
    auto& sym = vec[0];
    sym.val = blob + (uintptr_t)sym.val;
  }
}

// vim: set expandtab ts=2 sw=2 :
