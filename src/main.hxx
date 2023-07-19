#pragma once

#include <stddef.h>

void ShutdownTINE(int);
bool IsCmdLine();
char const* CmdLineBootText();

extern bool sanitize_clipboard;

#ifndef _WIN32
extern size_t page_size;
#endif
extern size_t proc_cnt;

// vim: set expandtab ts=2 sw=2 :
