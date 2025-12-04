#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Wrapper functions to redirect newer glibc __isoc23_* symbols to older versions
// This allows linking against libraries compiled with glibc 2.38+ on older systems

extern unsigned long long int __real_strtoull(const char *nptr, char **endptr, int base);
extern int __real_sscanf(const char *str, const char *format, ...);

unsigned long long int __wrap___isoc23_strtoull(const char *nptr, char **endptr, int base) {
    return strtoull(nptr, endptr, base);
}

int __wrap___isoc23_sscanf(const char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsscanf(str, format, args);
    va_end(args);
    return result;
}
