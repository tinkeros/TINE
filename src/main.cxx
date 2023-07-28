#ifdef _WIN32
  #include <windows.h>
  #include <winbase.h>
  #include <wincon.h>
  #include <winerror.h>
  #include <processenv.h>
  #include <processthreadsapi.h>

  // for mingw
  // https://archive.md/HEZm2#selection-3667.0-3698.0
  #ifndef ERROR_CONTROL_C_EXIT
    #define ERROR_CONTROL_C_EXIT 0x23C
  #endif

static BOOL WINAPI CtrlCHandlerRoutine(DWORD) {
  #define STR_(x) x, lstrlenA(x)
  WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE), STR_("User Abort.\n"), nullptr,
                nullptr);
  ExitProcess(ERROR_CONTROL_C_EXIT);
  return TRUE;
}

#else
  #include <signal.h>
  #include <sys/resource.h>
  #include <unistd.h>
#endif

#include <algorithm>
#include <filesystem>
#include <thread>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <argtable3.h>

#include "dbg.hxx"
#include "main.hxx"
#include "multic.hxx"
#include "runtime.hxx"
#include "sdl_window.hxx"
#include "sound.h"
#include "tos_aot.hxx"
#include "vfs.hxx"

namespace fs = std::filesystem;

using std::thread;

namespace {

constexpr bool is_win =
#ifdef _WIN32
    true;
#else
    false;
#endif

std::string boot_str;
std::string bin_path{"HCRT.BIN"};

int  exit_code = 0;
bool prog_exit = false;

void* __stdcall Core0(void*) {
  VFsThrdInit();
  LoadHCRT(bin_path);
  SetupDebugger();
#ifndef _WIN32
  signal(SIGUSR1, [](int) {
    pthread_exit(nullptr);
  });
#endif
  return nullptr;
}

} // namespace

char const* CmdLineBootText() {
  return boot_str.c_str();
}

void ShutdownTINE(int ec) {
  prog_exit = true;
  exit_code = ec;
  ShutdownCores(ec);
}

bool sanitize_clipboard = false;
bool is_cmd_line        = false;

#ifndef _WIN32
size_t page_size; // used for allocation
                  // and pointer checks
#else
DWORD dwAllocationGranularity;
#endif
size_t proc_cnt;

int main(int argc, char** argv) {
#ifndef _WIN32
  // https://archive.md/5cufN#selection-2369.223-2369.272
  // Hilarious how Linux manpages won't teach me anything
  // about why I got an EPERM when I raised rl.rlim_max
  struct rlimit rl;
  getrlimit(RLIMIT_NOFILE, &rl);
  rl.rlim_cur = rl.rlim_max;
  setrlimit(RLIMIT_NOFILE, &rl);
  page_size = sysconf(_SC_PAGESIZE);
#else
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  dwAllocationGranularity = si.dwAllocationGranularity;
#endif
  proc_cnt = thread::hardware_concurrency();
  // i wanted to use cli11 but i dont want exceptions in this codebase
  struct arg_lit *helpArg, *sixty_fps, *commandLineArg, *cb_sanitize, *ndebug,
      *noans;
  struct arg_file *cmdLineFiles, *TDriveArg, *HCRTArg;

  void* argtable[] = {
      helpArg        = arg_lit0("h", "help", "Display this help message"),
      sixty_fps      = arg_lit0("6", "60fps", "Run in 60 fps mode."),
      commandLineArg = arg_lit0("c", "com", "Command line mode, cwd -> Z:/"),
      cb_sanitize = arg_lit0("s", "sanitize-cb", "Sanitize clipboard contents"),
      ndebug      = arg_lit0("n", "ndebug", "Silence compiler output"),
      noans       = arg_lit0("a", "noans", "Silence expression results"),
      HCRTArg     = arg_file0("f", "file", nullptr, "Specify HolyC runtime"),
      TDriveArg   = arg_file0("t", "root", nullptr, "Specify boot folder"),
      cmdLineFiles = arg_filen(nullptr, nullptr, "<files>", 0, 100,
                               "Files that run on boot(cmdline mode specific)"),
      arg_end_(1),
  };
  int errs = arg_parse(argc, argv, argtable);
  if (helpArg->count > 0 || errs > 0 || TDriveArg->count == 0) {
    fprintf(stderr, "Usage: %s", argv[0]);
    arg_print_syntaxv(stderr, argtable, "\n");
    arg_print_glossary_gnu(stderr, argtable);
    return 1;
  }

  if (fs::exists(TDriveArg->filename[0])) {
    VFsMountDrive('T', TDriveArg->filename[0]);
  } else {
    fprintf(stderr, "%s DOES NOT EXIST\n", TDriveArg->filename[0]);
    return 1;
  }
  if (commandLineArg->count > 0) {
    VFsMountDrive('Z', ".");
    is_cmd_line = true;
  }
  if (cb_sanitize->count > 0)
    sanitize_clipboard = true;

  // This is called before LoadHCRT so TOSLoader will not be
  // all fucked up, fyi
  RegisterFuncPtrs();

  if (ndebug->count == 0)
    boot_str += "__EnableDbg;\n";
  if (noans->count == 0)
    boot_str += "__EnableAns;\n";
  if (is_cmd_line) {
    boot_str += "#exe {Drv('Z');};\n";
    for (int i = 0; i < cmdLineFiles->count; ++i) {
      boot_str += "#include \"";
      boot_str += cmdLineFiles->filename[i];
      boot_str += "\";\n";
    }
#ifdef _WIN32
    std::replace(boot_str.begin(), boot_str.end(), '\\', '/');
#endif
  }
  if (sixty_fps->count != 0)
    boot_str += "SetFPS(60.);\n";
  if (!is_cmd_line)
    NewDrawWindow();
  if (is_win || !is_cmd_line)
    InitSound();
  if (HCRTArg->count > 0)
    bin_path = HCRTArg->filename[0];
  if (fs::exists(bin_path)) {
    if (ndebug->count == 0)
      fprintf(stderr, "Using %s as the kernel.\n", bin_path.c_str());
  } else {
    fprintf(stderr, "%s DOES NOT EXIST\n", bin_path.c_str());
    return 1;
  }
  LaunchCore0(Core0);
  arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
  if (!is_cmd_line) {
#ifdef _WIN32
    SetConsoleCtrlHandler(CtrlCHandlerRoutine, TRUE);
#endif
    InputLoop(&prog_exit);
  } else {
    WaitForCore0();
  }
  return exit_code;
}

// vim: set expandtab ts=2 sw=2 :
