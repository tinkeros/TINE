#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>

void SetClipboard(char const* text);
std::string const ClipboardText();
struct CDrawWindow;
CDrawWindow* NewDrawWindow();
void DrawWindowUpdate(uint8_t* colors, uintptr_t internal_width);
void InputLoop(bool* off);
void GrPaletteColorSet(uint64_t, uint64_t);

void SetKBCallback(void* fp, void* data);
void SetMSCallback(void* fp);

// vim: set expandtab ts=2 sw=2 :
