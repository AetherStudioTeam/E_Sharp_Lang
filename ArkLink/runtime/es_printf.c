#include "es_standalone.h"

static void es_print_padded(const char* str, int width, int left_align, char pad_char) {
    int len = es_strlen(str);
    int pad = width - len;
    
    if (pad > 0 && !left_align) {
        while (pad-- > 0) es_putchar(pad_char);
    }
    
    es_print(str);
    
    if (pad > 0 && left_align) {
        while (pad-- > 0) es_putchar(pad_char);
    }
}

void es_printf(const char* format, ...) {
    if (!format) return;
    
    va_list args;
    va_start(args, format);
    es_vprintf(format, args);
    va_end(args);
}

void es_vprintf(const char* format, va_list args) {
    if (!format) return;
    
    char buf[64];
    
    while (*format) {
        if (*format != '%') {
            es_putchar(*format);
            format++;
            continue;
        }
        
        format++;
        
        int left_align = 0;
        int width = 0;
        int precision = -1;
        char pad_char = ' ';
        
        if (*format == '-') {
            left_align = 1;
            format++;
        }
        
        if (*format == '0') {
            pad_char = '0';
            format++;
        }
        
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format - '0');
            format++;
        }
        
        if (*format == '.') {
            format++;
            precision = 0;
            while (*format >= '0' && *format <= '9') {
                precision = precision * 10 + (*format - '0');
                format++;
            }
        }
        
        switch (*format) {
            case 'd':
            case 'i': {
                int val = va_arg(args, int);
                es_itoa(val, buf, 10);
                es_print_padded(buf, width, left_align, pad_char);
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                es_itoa((int)val, buf, 10);
                es_print_padded(buf, width, left_align, pad_char);
                break;
            }
            case 'x': {
                unsigned int val = va_arg(args, unsigned int);
                es_itoa((int)val, buf, 16);
                es_print_padded(buf, width, left_align, pad_char);
                break;
            }
            case 'X': {
                unsigned int val = va_arg(args, unsigned int);
                es_itoa((int)val, buf, 16);
                for (int i = 0; buf[i]; i++) {
                    if (buf[i] >= 'a' && buf[i] <= 'z') {
                        buf[i] = buf[i] - 'a' + 'A';
                    }
                }
                es_print_padded(buf, width, left_align, pad_char);
                break;
            }
            case 'f': {
                double val = va_arg(args, double);
                if (precision < 0) precision = 6;
                es_ftoa(val, buf, precision);
                es_print_padded(buf, width, left_align, pad_char);
                break;
            }
            case 'c': {
                char val = (char)va_arg(args, int);
                es_putchar(val);
                break;
            }
            case 's': {
                const char* val = va_arg(args, const char*);
                if (!val) val = "(null)";
                if (precision >= 0) {
                    int len = es_strlen(val);
                    if (len > precision) len = precision;
                    es_strncpy(buf, val, len);
                    buf[len] = '\0';
                    es_print_padded(buf, width, left_align, pad_char);
                } else {
                    es_print_padded(val, width, left_align, pad_char);
                }
                break;
            }
            case 'p': {
                void* val = va_arg(args, void*);
                es_itoa((int)(long long)val, buf, 16);
                es_print("0x");
                es_print(buf);
                break;
            }
            case '%':
                es_putchar('%');
                break;
            default:
                es_putchar('%');
                es_putchar(*format);
                break;
        }
        
        format++;
    }
}
