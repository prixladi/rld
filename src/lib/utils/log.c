
#include <stdarg.h>
#include <stdlib.h>

#include "log.h"
#include "memory.h"

static volatile enum log_level min_level = WARN;

void
log_init(enum log_level level)
{
    min_level = level;
}

void
log_critical(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    scoped char *fmt = str_concat("\x1B[31m[CRT]\x1B[0m ", format);
    vfprintf(stderr, fmt, args);

    va_end(args);
}

void
log_error(const char *format, ...)
{
    if (min_level > ERROR)
        return;

    va_list args;
    va_start(args, format);

    scoped char *fmt = str_concat("\x1B[31m[ERR]\x1B[0m ", format);
    vfprintf(stderr, fmt, args);

    va_end(args);
}

void
log_warn(const char *format, ...)
{
    if (min_level > WARN)
        return;

    va_list args;
    va_start(args, format);

    scoped char *fmt = str_concat("\x1B[33m[WRN]\x1B[0m ", format);
    vfprintf(stderr, fmt, args);

    va_end(args);
}

void
log_info(const char *format, ...)
{
    if (min_level > INFO)
        return;

    va_list args;
    va_start(args, format);

    scoped char *fmt = str_concat("\x1B[32m[INF]\x1B[0m ", format);
    vprintf(fmt, args);

    va_end(args);
}

void
log_debug(const char *format, ...)
{
    if (min_level > DEBUG)
        return;

    va_list args;
    va_start(args, format);

    scoped char *fmt = str_concat("\x1B[37m[DBG]\x1B[0m ", format);
    vprintf(fmt, args);

    va_end(args);
}

void
log_trace(const char *trace_name, const char *format, ...)
{
    if (min_level > TRACE)
        return;

    va_list args;
    va_start(args, format);

    scoped char *fmt = str_printf("\x1B[37m[TRC]\x1B[0m (%s) %s", trace_name, format);
    vprintf(fmt, args);

    va_end(args);
}
