#pragma once

#include <string>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define VFS_CDF_MAKE         1
#define VFS_CDF_FILENAME_ABS (1 << 1)

void VFsThrdInit();
void VFsSetDrv(char const d);
char VFsGetDrv();
void VFsSetPwd(char const* pwd);
bool VFsDirMk(char const* to, int const flags);
uint64_t VFsDel(char const* p);
std::string VFsFileNameAbs(char const* name);
int64_t VFsUnixTime(char const* name);
int64_t VFsFSize(char const* name);
uint64_t VFsFileWrite(char const* name, char const* data, size_t const len);
void* VFsFileRead(char const* name, uint64_t* len);
char** VFsDir();
bool VFsIsDir(char const* path);
bool VFsFileExists(char const* path);
void VFsMountDrive(char const let, char const* path);

// vim: set expandtab ts=2 sw=2 :
