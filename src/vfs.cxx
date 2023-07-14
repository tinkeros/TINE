#include "vfs.hxx"
#include "alloc.hxx"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using std::ios;

#include <ctype.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define delim '\\'
#else
#define delim '/'
#endif
// clang-format off
#include <sys/types.h>
#include <sys/stat.h>
// clang-format on

thread_local std::string thrd_pwd;
thread_local char thrd_drv;

void VFsThrdInit() {
  thrd_pwd = "/";
  thrd_drv = 'T';
}

void VFsSetDrv(char const d) {
  if (!isalpha(d))
    return;
  thrd_drv = toupper(d);
}

char VFsGetDrv() {
  return thrd_drv;
}

void VFsSetPwd(char const* pwd) {
  if (!pwd)
    pwd = "/";
  thrd_pwd = pwd;
}

static bool FExists(std::string const& path) {
  return fs::exists(path);
}

static int FIsDir(std::string const& path) {
  return fs::is_directory(path);
}

bool VFsDirMk(char const* to, int const flags) {
  std::string p = VFsFileNameAbs(to);
  if (FExists(p) && FIsDir(p)) {
    return true;
  } else if (flags & VFS_CDF_MAKE) {
    fs::create_directory(p);
    return true;
  }
  return false;
}

uint64_t VFsDel(char const* p) {
  std::string path = VFsFileNameAbs(p);
  bool e = FExists(path);
  if (!e)
    return 0;
  fs::remove_all(path);
  return 1;
}

static std::array<std::string, 'z' - 'a' + 1> mount_points;
std::string VFsFileNameAbs(char const* name) {
  std::string ret;
  // thrd_drv is always uppercase
  ret += mount_points[thrd_drv - 'A']; // T
  ret += delim;                        // /
  if (thrd_pwd.size() > 1) {
    ret.pop_back();
    ret += thrd_pwd; // /
    ret += delim;    // /
  }
  ret += name; // Name
  return ret;
}

int64_t VFsFSize(char const* name) {
  std::string fn = VFsFileNameAbs(name);
  if (!FExists(fn)) {
    return -1;
  } else if (FIsDir(fn)) {
    fs::directory_iterator it{fn};
    // https://archive.md/1Ojr3#7
    // accounts for . and ..
    return 2 + std::distance(fs::begin(it), fs::end(it));
  }
  return fs::file_size(fn);
}

uint64_t VFsUnixTime(char const* name) {
  std::string fn = VFsFileNameAbs(name);
  if (!FExists(fn))
    return 0;
#ifndef _WIN32
  struct stat s;
  stat(fn.c_str(), &s);
#else
  struct _stati64 s;
  _stati64(fn.c_str(), &s);
#endif
  return s.st_mtime;
}

uint64_t VFsFileWrite(char const* name, char const* data, size_t const len) {
  std::string p = VFsFileNameAbs(name);
  if (name) {
    std::ofstream f{p, ios::binary | ios::out};
    if (f)
      f.write(data, len);
  }
  return !!name;
}

void* VFsFileRead(char const* name, uint64_t* len) {
  if (len)
    *len = 0;
  if (!name)
    return nullptr;
  char* data = nullptr;
  std::string p = VFsFileNameAbs(name);
  if (!FExists(p))
    return nullptr;
  if (FIsDir(p))
    return nullptr;
  std::ifstream f{p, ios::binary | ios::in};
  if (!f)
    return nullptr;
  size_t sz = fs::file_size(p);
  f.read(data = HolyAlloc<char, true>(sz + 1), sz);
  if (len)
    *len = sz;
  return data;
}

char** VFsDir() {
  std::string file = VFsFileNameAbs("");
  if (!FIsDir(file))
    return nullptr;
#define SD_(s) HolyStrDup(s)
  // https://archive.md/1Ojr3#7
  using DirEnt = char*;
  std::vector<DirEnt> items{
      SD_("."),
      SD_(".."),
  };
  for (auto const& e : fs::directory_iterator{file}) {
    auto const& s = e.path().filename().string();
    // CDIR_FILENAME_LEN is 38(includes '\0')
    // do not touch, fat32 legacy
    // will break opening ISOs if touched
    if (s.size() <= 38 - 1)
      items.emplace_back(SD_(s.c_str()));
  }
  // force null pointer terminator
  auto ret = HolyAlloc<DirEnt, true>(items.size() + 1);
  std::copy(items.begin(), items.end(), ret);
  return ret;
}

bool VFsIsDir(char const* path) {
  return FIsDir(VFsFileNameAbs(path));
}

bool VFsFileExists(char const* path) {
  return FExists(VFsFileNameAbs(path));
}

void VFsMountDrive(char const let, char const* path) {
  mount_points[toupper(let) - 'A'] = path;
}

// vim: set expandtab ts=2 sw=2 :
