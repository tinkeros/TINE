#include "multic.hxx"
#include "dbg.hxx"
#include "ffi.h"
#include "runtime.hxx"
#include "tos_aot.hxx"
#include "vfs.hxx"

#include <atomic>
#include <vector>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <processthreadsapi.h>
#include <synchapi.h>
#include <sysinfoapi.h>
#include <timeapi.h>
using WinCB = LPTHREAD_START_ROUTINE;
// clang-format on
#else
#include <pthread.h>
#include <signal.h>
#endif

#ifdef __linux__
#include <linux/futex.h>
#include <sys/syscall.h>
#elif defined __FreeBSD__
#include <sys/types.h>
#include <sys/umtx.h>
#endif

uint64_t GetTicks() {
#ifdef _WIN32
  return GetTickCount();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)(ts.tv_nsec / 1000000) + 1000 * (uint64_t)ts.tv_sec;
#endif
}

/*
 * (DolDoc code)
 * $ID,-2$$TR-C,"How do you use the FS and GS segment registers."$
 * $ID,2$$FG,2$MOV RAX,FS:[RAX]$FG$ : FS can be set with a $FG,2$WRMSR$FG$, but
 * displacement is RIP relative, so it's tricky to use.  FS is used for the
 * current $LK,"CTask",A="MN:CTask"$, GS for $LK,"CCPU",A="MN:CCPU"$.
 *
 * Note on Fs and Gs: They might seem like very weird names for ThisTask and
 * ThisCPU repectively but it's because they are stored in the F Segment and G
 * Segment registers. (https://archive.md/pf2td)
 */

thread_local std::atomic<void*> Fs = nullptr;

void* GetFs() {
  return Fs;
}

void SetFs(void* f) {
  Fs = f;
}

thread_local std::atomic<void*> Gs = nullptr;

void* GetGs() {
  return Gs;
}

void SetGs(void* g) {
  Gs = g;
}

thread_local size_t core_num;
size_t CoreNum() {
  return core_num;
}

// have you ever died in a nightmare surprised you havent earned your fate?
struct CCore {
#ifdef _WIN32
  HANDLE thread;
  HANDLE event;
  HANDLE mtx;
  uint64_t awake_at;
#else
  pthread_t thread;
  /*
   * man 2 futex
   * > The uaddr argument points to the futex word.  On all platforms,
   * > futexes are four-byte integers that must be aligned on a four-
   * > byte boundary.
   * freebsd doesnt seem to mind about alignment so im just going to use
   * uint32_t too(though i have to specify UMTX_OP_WAIT_UINT instead of
   * UMTX_OP_WAIT)
   */
  alignas(4) uint32_t is_sleeping;
  // not using atomics here(instead using atomic builtins that operate on plain
  // values) because i need them for system calls and casting std::atomic<T>* to
  // T* is potentially UB and
  // static_assert(std::is_layout_compatible_v<std::atomic<uint32_t>, uint32_t>)
  // failed on my machine
#endif
  bool is_alive;
  void* fp;
};

static std::vector<CCore> cores;

static void* __stdcall LaunchCore(void* c) {
  VFsThrdInit();
  SetupDebugger();
  core_num = (uintptr_t)c;
#ifndef _WIN32
  static void* fp = nullptr;
  if (fp == nullptr)
    fp = TOSLoader["__InterruptCoreRoutine"][0].val;
  signal(SIGUSR2, (void (*)(int))fp);
  signal(SIGUSR1, [](int) {
    pthread_exit(nullptr);
  });
#endif
  // CoreAPSethTask(...) (T/FULL_PACKAGE.HC)
  FFI_CALL_TOS_0_ZERO_BP(cores[core_num].fp);
  return nullptr;
}

// this may look like bad code but HolyC cannot switch
// contexts unless you call Yield() in a loop so
// we have to set RIP manually(this routine is called
// when CTRL+ALT+C is pressed inside TempleOS
void InterruptCore(size_t core) {
#ifdef _WIN32
  CONTEXT ctx{};
  ctx.ContextFlags = CONTEXT_FULL;
  SuspendThread(cores[core].thread);
  GetThreadContext(cores[core].thread, &ctx);
  // push rip
  ctx.Rsp -= 8;
  ((DWORD64*)ctx.Rsp)[0] = ctx.Rip;
  //
  static void* fp = nullptr;
  if (fp == nullptr)
    fp = TOSLoader["__InterruptCoreRoutine"][0].val;
  // movabs rip, <fp>
  ctx.Rip = reinterpret_cast<uintptr_t>(fp);
  SetThreadContext(cores[core].thread, &ctx);
  ResumeThread(cores[core].thread);
#else
  pthread_kill(cores[core].thread, SIGUSR2);
#endif
}

void LaunchCore0(ThreadCallback* fp) {
  cores.resize(mp_cnt(nullptr));
  cores[0].fp = nullptr;
#ifdef _WIN32
  // sorry for this piece of utter garbage code, I wanted it to compile
  // without warnings
#define C_(x) reinterpret_cast<WinCB>((void*)x)
  cores[0].thread = CreateThread(nullptr, 0, C_(fp), nullptr, 0, nullptr);
  cores[0].mtx = CreateMutex(nullptr, FALSE, nullptr);
  cores[0].event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  SetThreadPriority(cores[0].thread, THREAD_PRIORITY_HIGHEST);
  cores[0].is_alive = true;
  // im not going to use SEH or some crazy bulllshit to set the
  // thread name on windows(https://archive.md/9jiD5)
#else
  pthread_create(&cores[0].thread, nullptr, fp, nullptr);
  pthread_setname_np(cores[0].thread, "Seth(Core0)");
#endif
}

void CreateCore(size_t core, void* fp) {
  // CoreAPSethTask(...) passed from SpawnCore
  cores[core].fp = fp;
#ifdef _WIN32
  cores[core].thread =
      CreateThread(nullptr, 0, C_(LaunchCore), (void*)core, 0, nullptr);
  cores[core].mtx = CreateMutex(nullptr, FALSE, nullptr);
  cores[core].event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  SetThreadPriority(cores[core].thread, THREAD_PRIORITY_HIGHEST);
  cores[core].is_alive = true;
#else
  pthread_create(&cores[core].thread, nullptr, LaunchCore, (void*)core);
  pthread_setname_np(cores[core].thread, "Seth");
#endif
}

void WaitForCore0() {
#ifdef _WIN32
  WaitForSingleObject(cores[0].thread, INFINITE);
#else
  pthread_join(cores[0].thread, nullptr);
#endif
}

void ShutdownCore(size_t core) {
#ifdef _WIN32
  TerminateThread(cores[core].thread, 0);
#else
  // you actually cant terminate a thread from core 0
  // with pthreads, you need some signal handler
  pthread_kill(cores[core].thread, SIGUSR1);
  pthread_join(cores[core].thread, nullptr);
#endif
}

void ShutdownCores(int ec) {
  for (size_t c = 0; c < mp_cnt(nullptr); ++c)
    if (c != core_num)
      ShutdownCore(c);
  // This is the same as calling Core0Exit
  // with the difference of being able to exit
  // with a specific exit code
  exit(ec);
}

void AwakeFromSleeping(size_t core) {
#ifdef _WIN32
  WaitForSingleObject(cores[core].mtx, INFINITE);
  cores[core].awake_at = 0;
  SetEvent(cores[core].event);
  ReleaseMutex(cores[core].mtx);
#else
  uint32_t old = 1;
  __atomic_compare_exchange_n(&cores[core].is_sleeping, &old, 0, false,
                              __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#ifdef __linux__
  syscall(SYS_futex, &cores[core].is_sleeping, FUTEX_WAKE, 1, nullptr, nullptr,
          0);
#elif defined(__FreeBSD__)
  _umtx_op(&cores[core].is_sleeping, UMTX_OP_WAKE, 1, nullptr, nullptr);
#endif
#endif
}

#ifdef _WIN32

static UINT tick_inc;
static uint64_t ticks = 0;

// To just get ticks we can use QueryPerformanceFrequency
// and QueryPerformanceCounter but we want to set an winmm
// event that updates the tick count while also helping cores wake up
//
// i killed two birds with one stoner
static uint64_t GetTicksHP() {
  static std::atomic<bool> init = false;
  if (!init) {
    init = true;
    TIMECAPS tc;
    timeGetDevCaps(&tc, sizeof tc);
    tick_inc = tc.wPeriodMin;
    timeSetEvent(
        tick_inc, tick_inc,
        [](auto, auto, auto, auto, auto) {
          ticks += tick_inc;
          for (size_t idx = 0; idx < mp_cnt(nullptr); ++idx) {
            auto& c = cores[idx];
            if (!c.is_alive)
              continue;
            WaitForSingleObject(c.mtx, INFINITE);
            // check if ticks reached awake_at
            if (ticks >= c.awake_at && c.awake_at > 0) {
              SetEvent(c.event);
              c.awake_at = 0;
            }
            ReleaseMutex(c.mtx);
          }
        },
        0, TIME_PERIODIC);
  }
  return ticks;
}
#endif

void SleepHP(uint64_t us) {
#ifdef _WIN32
  auto const s = GetTicksHP();
  WaitForSingleObject(cores[core_num].mtx, INFINITE);
  cores[core_num].awake_at = s + us / 1000;
  ReleaseMutex(cores[core_num].mtx);
  WaitForSingleObject(cores[core_num].event, INFINITE);
#else
  struct timespec ts {};
  ts.tv_nsec = us * 1000;
  __atomic_store_n(&cores[core_num].is_sleeping, 1, __ATOMIC_SEQ_CST);
#ifdef __linux__
  syscall(SYS_futex, &cores[core_num].is_sleeping, FUTEX_WAIT, 1, &ts, nullptr,
          0);
#elif defined(__FreeBSD__)
  _umtx_op(&cores[core_num].is_sleeping, UMTX_OP_WAIT_UINT, 1,
           (void*)sizeof(struct timespec), &ts);
#endif
#endif
}
