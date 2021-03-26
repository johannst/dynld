// Copyright (c) 2020 Johannes Stoelp

#include <fmt.h>

static const char* num2dec(char* buf, unsigned long len, unsigned long long num) {
    char* pbuf = buf + len - 1;
    *pbuf = '\0';

    if (num == 0) {
        *(--pbuf) = '0';
    }

    while (num > 0 && pbuf != buf) {
      char d = (char)(num % 10) + '0';
        *(--pbuf) = d;
        num /= 10;
    }
    return pbuf;
}

static const char* num2hex(char* buf, unsigned long len, unsigned long long num) {
    char* pbuf = buf + len - 1;
    *pbuf = '\0';

    if (num == 0) {
        *(--pbuf) = '0';
    }

    while (num > 0 && pbuf != buf) {
        char d = (num & 0xf);
        *(--pbuf) = (char)(d + (d > 9 ? 'a' - 10 : '0'));
        num >>= 4;
    }
    return pbuf;
}

int vfmt(char* buf, unsigned long len, const char* fmt, va_list ap) {
    unsigned i = 0;

#define put(c)           \
    {                    \
        char _c = (c);   \
        if (i < len) {   \
            buf[i] = _c; \
        }                \
        ++i;             \
    }

#define puts(s)    \
    while (*s) {   \
        put(*s++); \
    }

    char scratch[16];
    int l_cnt = 0;

    while (*fmt) {
        if (*fmt != '%') {
            put(*fmt++);
            continue;
        }

        l_cnt = 0;

    continue_fmt:
        switch (*(++fmt /* constume '%' */)) {
            case 'l':
                ++l_cnt;
                goto continue_fmt;
            case 'd': {
                long val = l_cnt > 0 ? va_arg(ap, long) : va_arg(ap, int);
                if (val < 0) {
                    val *= -1;
                    put('-');
                }
                const char* ptr = num2dec(scratch, sizeof(scratch), (unsigned long)val);
                puts(ptr);
            } break;
            case 'x': {
                unsigned long val = l_cnt > 0 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
                const char* ptr = num2hex(scratch, sizeof(scratch), val);
                puts(ptr);
            } break;
            case 's': {
                const char* ptr = va_arg(ap, const char*);
                puts(ptr);
            } break;
            case 'p': {
                const void* val = va_arg(ap, const void*);
                const char* ptr = num2hex(scratch, sizeof(scratch), (unsigned long long)val);
                put('0');
                put('x');
                puts(ptr);
            } break;
            default:
                put(*fmt);
                break;
        }
        ++fmt;
    }

#undef puts
#undef put

    if (buf) {
        i < len ? (buf[i] = '\0') : (buf[len - 1] = '\0');
    }
    return i;
}

int fmt(char* buf, unsigned long len, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vfmt(buf, len, fmt, ap);
    va_end(ap);
    return ret;
}
