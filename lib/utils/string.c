#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include "string.h"

#define min(x, y) (((x) >= (y)) ? (y) : (x))

static char *_str_dup(const char *str, size_t len);

char *
str_dup(const char *str)
{
    if (!str)
        return NULL;

    size_t len = strlen(str);
    return _str_dup(str, len);
}

char *
str_dup_maxlen(const char *str, size_t max_len)
{
    if (!str)
        return NULL;

    size_t len = min(strlen(str), max_len);
    return _str_dup(str, len);
}

char *
str_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    int size = vsnprintf(NULL, 0, format, args);
    char *buff = malloc(size + sizeof(char));

    va_end(args);
    va_start(args, format);

    vsnprintf(buff, size + 1, format, args);

    va_end(args);

    return buff;
}

char *
_str_concat(const char *fst, ...)
{
    va_list strings;

    int total_len = 0;
    const char *current = fst;

    va_start(strings, fst);
    do
    {
        total_len += strlen(current);
    } while ((current = va_arg(strings, const char *)));
    va_end(strings);

    char *result = malloc(total_len + 1);
    result[0] = '\0';
    current = fst;

    va_start(strings, fst);
    do
    {
        strcat(result, current);
    } while ((current = va_arg(strings, char *)));
    va_end(strings);

    return result;
}

bool
str_starts_with(const char *str, const char *prefix)
{
    if (!str || !prefix)
        return false;

    return strncmp(prefix, str, strlen(prefix)) == 0;
}

bool
str_ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return false;

    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);

    if (lensuffix > lenstr)
        return false;

    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

char *
int_to_str(int i)
{
    return str_printf("%d", i);
}

char *
unsigned_long_to_str(unsigned long i)
{
    return str_printf("%lu", i);
}

static char *
_str_dup(const char *str, size_t len)
{
    char *new_str = (char *)malloc(sizeof(char) * (len + 1));
    new_str[len] = '\0';

    memcpy(new_str, str, len);
    return new_str;
}
