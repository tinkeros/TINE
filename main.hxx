#pragma once

#include <stdint.h>

void ShutdownTINE(int32_t);
bool IsCmdLine();
char const* CmdLineBootText();

extern bool sanitize_clipboard;
