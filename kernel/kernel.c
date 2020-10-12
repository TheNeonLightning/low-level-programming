#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "interrupts.h"

enum vga_color {
  VGA_COLOR_BLACK = 0,
  VGA_COLOR_BLUE = 1,
  VGA_COLOR_GREEN = 2,
  VGA_COLOR_CYAN = 3,
  VGA_COLOR_RED = 4,
  VGA_COLOR_MAGENTA = 5,
  VGA_COLOR_BROWN = 6,
  VGA_COLOR_LIGHT_GREY = 7,
  VGA_COLOR_DARK_GREY = 8,
  VGA_COLOR_LIGHT_BLUE = 9,
  VGA_COLOR_LIGHT_GREEN = 10,
  VGA_COLOR_LIGHT_CYAN = 11,
  VGA_COLOR_LIGHT_RED = 12,
  VGA_COLOR_LIGHT_MAGENTA = 13,
  VGA_COLOR_LIGHT_BROWN = 14,
  VGA_COLOR_WHITE = 15,
};



static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
  return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
  return (uint16_t)uc | (uint16_t)color << 8;
}

size_t strlen(const char *str);

// dest	- pointer to the object to fill
// ch -	fill byte
// count - number of bytes to fill
void* memset(void* dest, int ch, size_t count);

// dest	- pointer to the object to copy to
// src - pointer to the object to copy from
// count - number of bytes to copy
void* memcpy(void* dest, const void* src, size_t count);

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

size_t terminal_row;
size_t terminal_column;
uint16_t* terminal_buffer;

void terminal_initialize(void) {

  terminal_row = 0;
  terminal_column = 0;

  uint8_t color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

  terminal_buffer = (uint16_t*)0xB8000;

  for (size_t y = 0; y < VGA_HEIGHT; ++y) {

    for (size_t x = 0; x < VGA_WIDTH; ++x) {
      const size_t index = y * VGA_WIDTH + x;
      terminal_buffer[index] = vga_entry(' ', color);
    }

  }
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar_color(char c, uint8_t color) {

  switch (c) {

  case '\n':
    terminal_row++;
    terminal_column = 0;
    if (terminal_row == VGA_HEIGHT) {
      terminal_row = 0;
    }
    break;

  default:
    terminal_putentryat(c, color, terminal_column, terminal_row);

    terminal_column++;
    if (terminal_column == VGA_WIDTH) {

      terminal_column = 0;
      terminal_row++;

      if (terminal_row == VGA_HEIGHT) {
        terminal_row = 0;
      }
    }
  }
}

void terminal_write(const char* data, size_t size, uint8_t color) {
  for (size_t i = 0; i < size; i++) {
    terminal_putchar_color(data[i], color);
  }
}

void terminal_write_number(const char* data) {
  for (size_t i = 0; i < strlen(data); i++) {
    terminal_putchar_color(data[i], vga_entry_color(VGA_COLOR_LIGHT_BLUE,
                                                          VGA_COLOR_BLACK));
  }
}

/*
void terminal_write_number(unsigned int number) {

  unsigned int number_cp = number,
               digits = 0;

  while (number_cp) {
    digits++;
    number_cp /= 10;
  }

  char number_str[digits + 1];

  number_str[digits] = '\0';
  for (int index = digits - 1; index >= 0; --index) {
    number_str[index] = number % 10 + '0';
    number /= 10;
  }

  terminal_write(number_str, strlen(number_str), vga_entry_color
      (VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
}
 */

void terminal_writestring(const char* data) {
  terminal_write(data, strlen(data),
                 vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

void terminal_writestring_color(const char* data, uint8_t color) {
  terminal_write(data, strlen(data), color);
}

// Interrupt Service Routines
__attribute__((interrupt)) void isr0(struct iframe* frame) {
  terminal_writestring_color("Hello world!\n",
                             vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
  (void)frame;
}

char* convert(unsigned int num, int base);

void printf(const char* format, ...);

void kernel_main(void) {
  // Initialize interrupt descriptor table
  init_idt();

  terminal_initialize();

  asm volatile("sti"); // Set interrupt flag (after the next instruction is
                       // executed)

  asm volatile("int $0x80"); // Pass control to interrupt vector 0x80

  printf("Function printf() test: %c, %d, %o, %s, %x. Test finished.",
         'a', 113, 702, "test string", 505);

  terminal_writestring_color("(c) carzil",
                             vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
}

////////////////////////////////////////////////////////////////////////////////
/// strlen, memset, memcpy /////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// convert, printf ////////////////////////////////////////////////////////////

char* convert(unsigned int num, int base)
{
  static char Representation[]= "0123456789ABCDEF";
  static char buffer[50];
  char* ptr;

  ptr = &buffer[49];
  *ptr = '\0';

  do {
    *--ptr = Representation[num%base];
    num /= base;
  } while (num != 0);

  return(ptr);
}

void printf(const char* format, ...) {

  va_list args;
  va_start(args, format);

  bool direct_output = true;

  int decimal = 0;
  unsigned int octal = 0,
      hexadecimal = 0;
  char* string = NULL;

  while (*format != '\0') {

    if (direct_output) {

      if (*format != '%') {
        terminal_putchar_color(*format, vga_entry_color(VGA_COLOR_GREEN,
                                                        VGA_COLOR_BLACK));
      } else {
        direct_output = false;
      }

    } else {

      switch (*format) {

      case 'c':
        terminal_putchar_color(va_arg(args, int), vga_entry_color
                               (VGA_COLOR_GREEN, VGA_COLOR_BLACK));
        break;

      case 'd':

        decimal = va_arg(args, int); // Fetch Decimal argument

        if (decimal < 0) {
          decimal = -decimal;
          terminal_putchar_color(
              '-', vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
        }

        terminal_write_number(convert(decimal, 10));
        break;

      case 'o':
        octal = va_arg(args, unsigned int); // Fetch Octal
        // representation
        terminal_write_number(convert(octal, 8));
        break;

      case 's':
        string = va_arg(args, char*); // Fetch string
        terminal_writestring(string);
        break;

      case 'x':
        hexadecimal = va_arg(args, unsigned int); // Fetch
        // Hexadecimal
        // representation
        terminal_write_number(convert(hexadecimal, 16));
        break;
      }

      direct_output = true;
    }
    ++format;
  }
}

////////////////////////////////////////////////////////////////////////////////
