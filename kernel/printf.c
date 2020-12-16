#include "printf.h"

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
