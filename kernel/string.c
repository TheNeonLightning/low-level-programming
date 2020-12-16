#pragma once

#include "string.h"

size_t strlen(const char *str) {
  size_t length = 0;

  while (str[length]) {
    ++length;
  }

  return length;
}

void* memset(void* dest, int ch, size_t count) {
  unsigned char u_ch = (unsigned char) ch;
  unsigned char* u_ch_dest = dest;

  for (size_t index = 0; index < count; ++index) {
    u_ch_dest[index] = u_ch;
  }

  return u_ch_dest;
}

void* memcpy(void* dest, const void* src, size_t count) {

  unsigned char* u_ch_dest = dest;
  const unsigned char* u_ch_src = src;

  for (size_t index = 0; index < count; ++index) {
    u_ch_dest[index] = u_ch_src[index];
  }

  return u_ch_dest;
}
