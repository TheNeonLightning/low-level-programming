#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

size_t strlen(const char *str);

// dest	- pointer to the object to fill
// ch -	fill byte
// count - number of bytes to fill
void* memset(void* dest, int ch, size_t count);

// dest	- pointer to the object to copy to
// src - pointer to the object to copy from
// count - number of bytes to copy
void* memcpy(void* dest, const void* src, size_t count);