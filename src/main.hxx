#pragma once

#include <stddef.h>

void        ShutdownTINE(int);
char const* CmdLineBootText();

extern bool sanitize_clipboard;
extern bool is_cmd_line;

#ifndef _WIN32
extern size_t page_size;
#endif
extern size_t proc_cnt;

// vim: set expandtab ts=2 sw=2 :
