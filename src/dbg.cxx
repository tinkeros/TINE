#include "dbg.hxx"
#include "ffi.h"
#include "tos_aot.hxx"

#ifdef _WIN32

// clang-format off
#include <windows.h>
#include <errhandlingapi.h>
// clang-format on

namespace {

LONG WINAPI VectorHandler(struct _EXCEPTION_POINTERS* info) {
  auto c = info->ExceptionRecord->ExceptionCode;
  switch (c) {
  #define FERR(code)     \
  case EXCEPTION_##code: \
    break;
    FERR(ACCESS_VIOLATION);
    FERR(ARRAY_BOUNDS_EXCEEDED);
    FERR(DATATYPE_MISALIGNMENT);
    FERR(FLT_DENORMAL_OPERAND);
    FERR(FLT_DIVIDE_BY_ZERO);
    FERR(FLT_INEXACT_RESULT);
    FERR(FLT_INVALID_OPERATION);
    FERR(FLT_OVERFLOW);
    FERR(FLT_STACK_CHECK);
    FERR(FLT_UNDERFLOW);
    FERR(ILLEGAL_INSTRUCTION);
    FERR(IN_PAGE_ERROR);
    FERR(INT_DIVIDE_BY_ZERO);
    FERR(INVALID_DISPOSITION);
    FERR(STACK_OVERFLOW);
    FERR(BREAKPOINT);
  // https://archive.md/sZzVj
  case STATUS_SINGLE_STEP:
    break;
  default:
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  CONTEXT* ctx = info->ContextRecord;
  #define REG(x) ctx->x
  uint64_t regs[] = {
      REG(Rax),    REG(Rcx), REG(Rdx),
      REG(Rbx),    REG(Rsp), REG(Rbp),
      REG(Rsi),    REG(Rdi), REG(R8),
      REG(R9),     REG(R10), REG(R11),
      REG(R12),    REG(R13), REG(R14),
      REG(R15),    REG(Rip), (uintptr_t)&ctx->FltSave,
      REG(EFlags),
  };
  uint64_t sig = (c == EXCEPTION_BREAKPOINT || c == STATUS_SINGLE_STEP)
                   ? 5 /* SIGTRAP */
                   : 0;
  static void* fp = nullptr;
  if (fp == nullptr)
    fp = TOSLoader["DebuggerLandWin"].val;
  FFI_CALL_TOS_2(fp, sig, reinterpret_cast<uintptr_t>(regs));
  return EXCEPTION_CONTINUE_EXECUTION;
}
} // namespace

void SetupDebugger() {
  AddVectoredExceptionHandler(1, &VectorHandler);
}

#else

  #include <signal.h>
  #include <ucontext.h>

  #include <initializer_list>

namespace {

void routine(int sig, siginfo_t*, ucontext_t* ctx) {
  BackTrace();
  uint64_t sig_i64 = sig;
  #ifdef __linux__
    #define REG(x) static_cast<uint64_t>(ctx->uc_mcontext.gregs[REG_##x])
  // clang-format off
  //
  // apparently ucontext is implementation defined idk
  // if your on musl or something fix this yourself and send me a patch
  // probably only works on glibc lmao
  // heres why i dont take the address of fpregs on linux
  // https://github.com/bminor/glibc/blob/4290aed05135ae4c0272006442d147f2155e70d7/sysdeps/unix/sysv/linux/x86/sys/ucontext.h#L239
  //
  // clang-format on
  uint64_t regs[] = {
      REG(RAX), REG(RCX), REG(RDX),
      REG(RBX), REG(RSP), REG(RBP),
      REG(RSI), REG(RDI), REG(R8),
      REG(R9),  REG(R10), REG(R11),
      REG(R12), REG(R13), REG(R14),
      REG(R15), REG(RIP), (uintptr_t)ctx->uc_mcontext.fpregs,
      REG(EFL),
  };
  #elif defined(__FreeBSD__)
    #define REG(X) static_cast<uint64_t>(ctx->uc_mcontext.mc_##X)
  // freebsd seems to just use an
  // array of longs for their floating point context lmao
  uint64_t regs[] = {
      REG(rax),    REG(rcx), REG(rdx),
      REG(rbx),    REG(rsp), REG(rbp),
      REG(rsi),    REG(rdi), REG(r8),
      REG(r9),     REG(r10), REG(r11),
      REG(r12),    REG(r13), REG(r14),
      REG(r15),    REG(rip), (uintptr_t)&ctx->uc_mcontext.mc_fpstate,
      REG(rflags),
  };
  #endif
  static void* fp = nullptr;
  if (fp == nullptr)
    fp = TOSLoader["DebuggerLand"].val;
  FFI_CALL_TOS_2(fp, sig_i64, reinterpret_cast<uintptr_t>(regs));
}
} // namespace

void SetupDebugger() {
  struct sigaction inf;
  inf.sa_flags = SA_SIGINFO | SA_NODEFER;
  // ugly piece of shit
  inf.sa_sigaction = (void (*)(int, siginfo_t*, void*))routine;
  sigemptyset(&inf.sa_mask);
  for (auto i : {SIGTRAP, SIGBUS, SIGSEGV, SIGFPE})
    sigaction(i, &inf, nullptr);
}

#endif

// vim: set expandtab ts=2 sw=2 :
