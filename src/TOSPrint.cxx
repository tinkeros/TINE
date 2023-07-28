#include <algorithm>
#include <string>

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "TOSPrint.hxx"

namespace {
char* UnescapeString(char* __restrict str, char* __restrict where) {
  while (*str) {
    char const* __restrict to;
    switch (*str) {
#define ESC(c, e) \
  case c:         \
    to = e;       \
    break
      ESC('\\', "\\\\");
      ESC('\a', "\\a");
      ESC('\b', "\\b");
      ESC('\f', "\\f");
      ESC('\n', "\\n");
      ESC('\r', "\\r");
      ESC('\t', "\\t");
      ESC('\v', "\\v");
      ESC('\"', "\\\"");
    default:
      goto check_us_key;
    }
    std::copy(to, to + 2, where);
    where += 2;
    ++str;
    continue;

  check_us_key:; // you bear a striking resemblance
                 // you look just like my bathroom mirror
    if (isalnum(static_cast<char unsigned>(*str)) == 0 &&
        strchr(" ~!@#$%^&*()_+|{}[]\\;':\",./<>?", *str) == nullptr) {
      // Note: this was giving me bizarre buffer overflow
      // errors and it turns out you MUST use uint8_t when
      // printing a 8 bit wide octal value to get the correct digits
      // probably it's typical GNU bullshittery or there's something
      // deep inside the Standard that I'm missing, either way, this works
      char buf[5];
      snprintf(buf, sizeof buf, "\\%" PRIo8, static_cast<uint8_t>(*str));
      std::copy(buf, buf + 4, where);
      where += 4;
      ++str;
      continue;
    }
    // default when matches none of the criteria above
    *where++ = *str++;
  }
  *where = '\0';
  return where;
}

std::string MStrPrint(char const* fmt, uint64_t, int64_t* argv) {
  // this does not compare argument count(argc)
  // with StrOcc(fmt, '%'), be careful i guess
  // it also isn't a fully featured one but should
  // account for most use cases
  char const *start = fmt, *end;
  std::string ret;
  int64_t     arg = -1;
loop:;
  ++arg;
  end = strchr(start, '%');
  if (end == nullptr)
    end = start + strlen(start);
  ret.append(start, end - start);
  if (*end == '\0')
    return ret;
  start = end + 1;
  if (*start == '-')
    ++start;
  /* this skips output format specifiers
   * because i dont think a debug printer
   * needs such a thing */
  // int64_t width = 0, decimals = 0;
  while (isdigit(*start)) {
    // width *= 10;
    // width += *start - '0';
    ++start;
  }
  if (*start == '.')
    ++start;
  while (isdigit(*start)) {
    // decimals *= 10;
    // decimals += *start - '0';
    ++start;
  }
  while (strchr("t,$/", *start) != nullptr)
    ++start;
  int64_t aux = 1;
  if (*start == '*') {
    aux = argv[arg++];
    ++start;
  } else if (*start == 'h') {
    while (isdigit(*start)) {
      aux *= 10;
      aux += *start - '0';
      ++start;
    }
  }
#define FMT_CH(x, T)                                                           \
  do {                                                                         \
    size_t sz  = snprintf(nullptr, 0, "%" x, reinterpret_cast<T*>(argv)[arg]); \
    char*  tmp = new (std::nothrow) char[sz + 1];                              \
    snprintf(tmp, sz + 1, "%" x, reinterpret_cast<T*>(argv)[arg]);             \
    ret += tmp;                                                                \
    delete[] tmp;                                                              \
  } while (false);
  switch (*start) {
  case 'd':
  case 'i':
    FMT_CH(PRId64, int64_t);
    break;
  case 'u':
    FMT_CH(PRIu64, uint64_t);
    break;
  case 'o':
    FMT_CH(PRIo64, uint64_t);
    break;
  case 'n':
    static_assert(sizeof(double) == sizeof(uint64_t));
    FMT_CH("f", double);
    break;
  case 'p':
    FMT_CH("p", void*);
    break;
  case 'c': {
    while (--aux >= 0) {
      auto chr = reinterpret_cast<uint64_t*>(argv)[arg];
      // this accounts for HolyC's multichar character literals too
      while (chr > 0) {
        uint8_t c = chr & 0xff;
        chr >>= 8;
        if (c > 0)
          ret += static_cast<char>(c);
      }
    }
  } break;
  case 's': {
    while (--aux >= 0)
      ret += reinterpret_cast<char**>(argv)[arg];
  } break;
  case 'q': {
    char *str = reinterpret_cast<char**>(argv)[arg],
         *buf = new (std::nothrow) char[strlen(str) * 4 + 1];
    UnescapeString(str, buf);
    ret += buf;
    delete[] buf;
    break;
  }
  case '%':
    ret += '%';
    break;
  default:;
  }
  ++start;
  goto loop;
}
} // namespace

void TOSPrint(char const* fmt, uint64_t argc, int64_t* argv) {
  auto s = MStrPrint(fmt, argc, argv);
  fputs(s.c_str(), stderr);
  fflush(stderr);
}

// vim: set expandtab ts=2 sw=2 :
