#pragma once

#include <stdint.h>

void ShutdownTINE(int);
bool IsCmdLine();
char const* CmdLineBootText();

extern bool sanitize_clipboard;
