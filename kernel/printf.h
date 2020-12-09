#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "vga.h"

char* convert(unsigned int num, int base);

void printf(const char* format, ...);
