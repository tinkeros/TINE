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

#include <chrono>
#include <filesystem>

#include <stddef.h>
#include <string.h>

#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <winbase.h>
#include <memoryapi.h>
// clang-format on
#else
#include <signal.h>
#include <sys/mman.h>
#endif

#include "dyad.h"
#include "linenoise-ng/linenoise.h"

void HolyFree(void* ptr) {
  static void* fptr = nullptr;
  if (!fptr)
    fptr = TOSLoader["_FREE"].val;
  FFI_CALL_TOS_1(fptr, reinterpret_cast<uintptr_t>(ptr));
}

void* HolyMAlloc(size_t sz) {
  static void* fptr = nullptr;
  if (!fptr)
    fptr = TOSLoader["_MALLOC"].val;
  return reinterpret_cast<void*>(FFI_CALL_TOS_2(fptr, sz, 0 /*NULL*/));
}

void* HolyCAlloc(size_t sz) {
  auto ret = HolyAlloc<uint8_t>(sz);
  std::fill(ret, ret + sz, 0);
  return ret;
}

char* HolyStrDup(char const* str) {
  return strcpy(HolyAlloc<char>(strlen(str) + 1), str);
}

size_t mp_cnt(void*) {
  return proc_cnt;
}

namespace { // ffi shit goes here

namespace fs = std::filesystem;
namespace chrono = std::chrono;

using chrono::system_clock;

void STK_DyadInit() {
  static bool init = false;
  if (init)
    return;
  init = true;
  dyad_init();
  dyad_setUpdateTimeout(0.);
}

void STK_DyadUpdate() {
  dyad_update();
}

void STK_DyadShutdown() {
  dyad_shutdown();
}

void* STK_DyadNewStream() {
  return dyad_newStream();
}

int64_t STK_DyadListen(intptr_t* stk) {
  return dyad_listen(reinterpret_cast<dyad_Stream*>(stk[0]),
                     static_cast<int>(stk[1]));
}

int64_t STK_DyadConnect(intptr_t* stk) {
  return dyad_connect(reinterpret_cast<dyad_Stream*>(stk[0]),
                      reinterpret_cast<char*>(stk[1]),
                      static_cast<int>(stk[2]));
}

void STK_DyadWrite(intptr_t* stk) {
  dyad_write(reinterpret_cast<dyad_Stream*>(stk[0]),
             reinterpret_cast<void*>(stk[1]), static_cast<int>(stk[2]));
}

void STK_DyadEnd(dyad_Stream** stk) {
  dyad_end(stk[0]);
}

void STK_DyadClose(dyad_Stream** stk) {
  dyad_close(stk[0]);
}

char* STK_DyadGetAddress(dyad_Stream** stk) {
  char const* ret = dyad_getAddress(stk[0]);
  return HolyStrDup(ret);
}

void DyadReadCB(dyad_Event* e) {
  FFI_CALL_TOS_4(e->udata, reinterpret_cast<uintptr_t>(e->stream),
                 reinterpret_cast<uintptr_t>(e->data), e->size,
                 reinterpret_cast<uintptr_t>(e->udata2));
}

void STK_DyadSetReadCallback(void** stk) {
  // This is for a line of text
  dyad_addListener(static_cast<dyad_Stream*>(stk[0]), DYAD_EVENT_LINE,
                   DyadReadCB, stk[1], stk[2]);
}

void STK_DyadSetDataCallback(void** stk) {
  // This is for binary data
  dyad_addListener(static_cast<dyad_Stream*>(stk[0]), DYAD_EVENT_DATA,
                   DyadReadCB, stk[1], stk[2]);
}

void DyadListenCB(dyad_Event* e) {
  FFI_CALL_TOS_2(e->udata, reinterpret_cast<uintptr_t>(e->remote),
                 reinterpret_cast<uintptr_t>(e->udata2));
}

void DyadCloseCB(dyad_Event* e) {
  FFI_CALL_TOS_2(e->udata, reinterpret_cast<uintptr_t>(e->stream),
                 reinterpret_cast<uintptr_t>(e->udata2));
}

void STK_DyadSetOnCloseCallback(void** stk) {
  dyad_addListener(static_cast<dyad_Stream*>(stk[0]), DYAD_EVENT_CLOSE,
                   DyadCloseCB, stk[1], stk[2]);
}

void STK_DyadSetOnConnectCallback(void** stk) {
  dyad_addListener(static_cast<dyad_Stream*>(stk[0]), DYAD_EVENT_CONNECT,
                   DyadListenCB, stk[1], stk[2]);
}

void STK_DyadSetOnDestroyCallback(void** stk) {
  dyad_addListener(static_cast<dyad_Stream*>(stk[0]), DYAD_EVENT_DESTROY,
                   DyadCloseCB, stk[1], stk[2]);
}

void STK_DyadSetOnErrorCallback(void** stk) {
  dyad_addListener(static_cast<dyad_Stream*>(stk[0]), DYAD_EVENT_ERROR,
                   DyadCloseCB, stk[1], stk[2]);
}

void STK_DyadSetOnReadyCallback(void** stk) {
  dyad_addListener(static_cast<dyad_Stream*>(stk[0]), DYAD_EVENT_READY,
                   DyadListenCB, stk[1], stk[2]);
}

void STK_DyadSetOnTickCallback(void** stk) {
  dyad_addListener(static_cast<dyad_Stream*>(stk[0]), DYAD_EVENT_TICK,
                   DyadListenCB, stk[1], stk[2]);
}

void STK_DyadSetOnTimeoutCallback(void** stk) {
  dyad_addListener(static_cast<dyad_Stream*>(stk[0]), DYAD_EVENT_TIMEOUT,
                   DyadListenCB, stk[1], stk[2]);
}

void STK_DyadSetOnListenCallback(void** stk) {
  dyad_addListener(static_cast<dyad_Stream*>(stk[0]), DYAD_EVENT_ACCEPT,
                   DyadListenCB, stk[1], stk[2]);
}

void STK_DyadSetTimeout(uintptr_t* stk) {
  static_assert(sizeof(double) == sizeof(uint64_t));
  dyad_setTimeout(reinterpret_cast<dyad_Stream*>(stk[0]),
                  reinterpret_cast<double*>(stk)[1]);
}

void STK_DyadSetNoDelay(intptr_t* stk) {
  dyad_setNoDelay(reinterpret_cast<dyad_Stream*>(stk[0]),
                  static_cast<int>(stk[1]));
}

void STK_UnblockSignals() {
#ifndef _WIN32
  sigset_t all;
  sigfillset(&all);
  sigprocmask(SIG_UNBLOCK, &all, nullptr);
#endif
}

void STK__GrPaletteColorSet(uint64_t* stk) {
  GrPaletteColorSet(stk[0], {stk[1]});
}

uint64_t STK___IsValidPtr(uintptr_t* stk) {
#ifdef _WIN32
  // Wine doesnt like the
  // IsBadReadPtr,so use a polyfill

  // wtf IsBadReadPtr gives me a segfault so i just have to use this
  // polyfill lmfao
  // #ifdef __WINE__
  MEMORY_BASIC_INFORMATION mbi{};
  if (VirtualQuery(reinterpret_cast<void*>(stk[0]), &mbi, sizeof mbi)) {
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
  /* #ifdef __FreeBSD__ */
  // round down to page boundary (equiv to stk[0] / page_size * page_size)
  //   0b100101010 (x)
  // & 0b111110000 <- ~(0b10000 - 1)
  // --------------
  //   0b100100000
  uintptr_t addr = stk[0] & ~(page_size - 1);
  // https://archive.md/Aj0S4
  return -1 != msync(reinterpret_cast<void*>(addr), page_size, MS_ASYNC);
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

void STK_InterruptCore(uint64_t* stk) {
  InterruptCore(stk[0]);
}

void STK___BootstrapForeachSymbol(void** stk) {
  for (auto& [name, sym] : TOSLoader)
    FFI_CALL_TOS_3(stk[0], reinterpret_cast<uintptr_t>(name.c_str()),
                   reinterpret_cast<uintptr_t>(sym.val),
                   sym.type == HTT_EXPORT_SYS_SYM ? HTT_FUN : sym.type);
}

void STK_TOSPrint(intptr_t* stk) {
  TOSPrint(reinterpret_cast<char*>(stk[0]), stk[1], stk + 2);
}

void STK_DrawWindowUpdate(uintptr_t* stk) {
  DrawWindowUpdate(reinterpret_cast<uint8_t*>(stk[0]), stk[1]);
}

uint64_t STK___GetTicksHP(void*) {
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

uint64_t STK___GetTicks(void*) {
  return GetTicks();
}

void STK_SetKBCallback(void** stk) {
  SetKBCallback(stk[0], stk[1]);
}

void STK_SetMSCallback(void** stk) {
  SetMSCallback(stk[0]);
}

void STK___AwakeCore(size_t* stk) {
  AwakeFromSleeping(stk[0]);
}

void STK___SleepHP(uint64_t* stk) {
  SleepHP(stk[0]);
}

void STK___Sleep(uint64_t* stk) {
  SleepHP(stk[0] * 1000);
}

void STK_SetFs(void** stk) {
  SetFs(stk[0]);
}

void STK_SetGs(void** stk) {
  SetGs(stk[0]);
}

void STK_SndFreq(uint64_t* stk) {
  SndFreq(stk[0]);
}

void STK_SetClipboardText(char** stk) {
  SetClipboard(stk[0]);
}

char* STK___GetStr(char** stk) {
  char *s = linenoise(stk[0]), *r;
  if (s == nullptr)
    return nullptr;
  linenoiseHistoryAdd(s);
  r = HolyStrDup(s);
  free(s);
  return r;
}

char* STK_GetClipboardText(void*) {
  std::string clip{ClipboardText()};
  return HolyStrDup(clip.c_str());
}

uint64_t STK_FUnixTime(char** stk) {
  return VFsUnixTime(stk[0]);
}

void STK_VFsFTrunc(uintptr_t* stk) {
  fs::resize_file(VFsFileNameAbs(reinterpret_cast<char*>(stk[0])), stk[1]);
}

uint64_t STK___FExists(char** stk) {
  return VFsFileExists(stk[0]);
}

uint64_t STK_UnixNow(void*) {
  return system_clock::to_time_t(system_clock::now());
}

void STK___SpawnCore(uintptr_t* stk) {
  CreateCore(stk[0], reinterpret_cast<void*>(stk[1]));
}

void* STK_NewVirtualChunk(size_t* stk) {
  return NewVirtualChunk(stk[0], stk[1]);
}

void STK_FreeVirtualChunk(uintptr_t* stk) {
  FreeVirtualChunk(reinterpret_cast<void*>(stk[0]), stk[1]);
}

void STK_VFsSetPwd(char** stk) {
  VFsSetPwd(stk[0]);
}

uint64_t STK_VFsExists(char** stk) {
  return VFsFileExists(stk[0]);
}

uint64_t STK_VFsIsDir(char** stk) {
  return VFsIsDir(stk[0]);
}

int64_t STK_VFsFSize(char** stk) {
  return VFsFSize(stk[0]);
}

void* STK_VFsFRead(char** stk) {
  return VFsFileRead(stk[0], reinterpret_cast<uint64_t*>(stk[1]));
}

uint64_t STK_VFsFWrite(char** stk) {
  return VFsFileWrite(stk[0], stk[1], reinterpret_cast<uintptr_t>(stk[2]));
}

uint64_t STK_VFsDirMk(char** stk) {
  return VFsDirMk(stk[0], VFS_CDF_MAKE);
}

char** STK_VFsDir(void*) {
  return VFsDir();
}

uint64_t STK_VFsDel(char** stk) {
  return VFsDel(stk[0]);
}

FILE* VFsFOpen(char const* path, char const* m) {
  std::string p = VFsFileNameAbs(path);
  return fopen(p.c_str(), m);
}

FILE* STK_VFsFOpenW(char** stk) {
  return VFsFOpen(stk[0], "w+b");
}

FILE* STK_VFsFOpenR(char** stk) {
  return VFsFOpen(stk[0], "rb");
}

void STK_VFsFClose(FILE** stk) {
  fclose(stk[0]);
}

uint64_t STK_VFsFBlkRead(uintptr_t* stk) {
  fflush(reinterpret_cast<FILE*>(stk[3]));
  return stk[2] == fread(reinterpret_cast<void*>(stk[0]), stk[1], stk[2],
                         reinterpret_cast<FILE*>(stk[3]));
}

uint64_t STK_VFsFBlkWrite(uintptr_t* stk) {
  bool r = stk[2] == fwrite(reinterpret_cast<void*>(stk[0]), stk[1], stk[2],
                            reinterpret_cast<FILE*>(stk[3]));
  fflush(reinterpret_cast<FILE*>(stk[3]));
  return r;
}

void STK_VFsFSeek(uintptr_t* stk) {
  fseek(reinterpret_cast<FILE*>(stk[1]), stk[0], SEEK_SET);
}

void STK_VFsSetDrv(char* stk) {
  VFsSetDrv(stk[0]);
}

uint64_t STK_VFsGetDrv(void*) {
  return static_cast<uint64_t>(VFsGetDrv());
}

void STK_SetVolume(uint64_t* stk) {
  union {
    uint64_t i;
    double flt;
  } un = {stk[0]};
  SetVolume(un.flt);
}

uint64_t STK_GetVolume(void*) {
  union {
    double flt;
    uint64_t i;
  } un = {GetVolume()};
  return un.i;
}

void STK_ExitTINE(int* stk) {
  ShutdownTINE(stk[0]);
}

// arity must be <= 0xffFF/sizeof U64
void RegisterFunctionPtr(std::string& blob, char const* name, uintptr_t fp,
                         uint16_t arity) {
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
  // rcx is the first arg i have to provide in win64
  // there used to be additional code to push&pop 4 imaginary stack args
  // to provide "register homes" that windows functions expect but
  // it seems like individual functions are required to provide
  // space for their own reg homes so we dont have to
  // (which does actually make sense)
  blob.append("\x48\x8D\x4D\x10", 4);
#else // sysv
  // rdi is the first arg i have to provide in sysv
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
  sym.val = reinterpret_cast<uint8_t*>(off);
  TOSLoader[name] = sym;
}

} // namespace

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
  S_(VFsGetDrv, 0);
  S_(GetVolume, 0);
  S_(SetVolume, 1);
  S_(__GetTicksHP, 0);
  S_(_GrPaletteColorSet, 2);
  auto blob = VirtAlloc<char>(ffi_blob.size());
  std::copy(ffi_blob.begin(), ffi_blob.end(), blob);
  for (auto& [name, sym] : TOSLoader)
    sym.val += reinterpret_cast<ptrdiff_t>(blob);
}

// vim: set expandtab ts=2 sw=2 :
