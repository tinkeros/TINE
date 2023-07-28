#pragma once

#include <string>

#include <stddef.h>
#include <stdint.h>

void        SetClipboard(char const* text);
std::string ClipboardText();
void        NewDrawWindow();
void        DrawWindowUpdate(uint8_t* colors, uintptr_t internal_width);
void        InputLoop(bool* off);

extern "C" union bgr_48 {
  uint64_t i;
  struct [[gnu::packed]] {
    uint16_t b, g, r, pad;
  };
};
void GrPaletteColorSet(uint64_t, bgr_48);

void SetKBCallback(void* fp, void* data);
void SetMSCallback(void* fp);

// vim: set expandtab ts=2 sw=2 :
