#include "utils/vector.h"
#include "utils/string.h"
#include "utils/memory.h"

#include "helpers.h"

bool
file_has_extension(const char *file, const char *extension)
{
    if (!extension || !strlen(extension))
        return false;

    // TODO: there is no need to malloc memory, but im lazy so i will solve it some other time
    scoped char *dotted_extension = extension[0] == '.' ? str_dup(extension) : str_printf("%c%s", '.', extension);

    if (strlen(file) <= strlen(dotted_extension))
        return false;
    return str_ends_with(file, dotted_extension);
}

bool
path_contains_subpath(const char *path, const char *_sp, bool from_start)
{
    int a_from = 0;
    size_t a_len = strlen(_sp);

    if (a_len > 0 && _sp[0] == '/')
        a_from++;
    if (a_len > 0 && _sp[a_len - 1] == '/')
        a_len--;

    scoped char *apex_subpath = str_dup_maxlen(_sp + a_from, a_len);

    char *idx = strstr(path, apex_subpath);

    if (idx == NULL)
        return false;

    if (from_start)
    {
        bool is_start = idx == path;
        bool is_first_in_path = (idx - 1) == path && *(idx - 1) == '/';
        bool is_first_in_relative_path = (idx - 2) == path && *(idx - 1) == '/' && *(idx - 2) == '.';

        if (!is_start && !is_first_in_path && !is_first_in_relative_path)
            return false;
    }

    if (idx != path && *(idx - 1) != '/')
    {
        return false;
    }

    return strlen(idx) == strlen(apex_subpath) || idx[strlen(apex_subpath)] == '/';
}

int
args_count_flag(struct args *args, char f)
{
    int count = 0;
    vec_for_each2(char, c, args->flags)
    {
        if (f == *c)
            count++;
    }

    return count;
}

int
args_count_long_flag(struct args *args, char *f)
{
    int count = 0;
    vec_for_each2(char *, str, args->long_flags)
    {
        if (strcmp(f, *str))
            count++;
    }

    return count;
}

int
args_count_value(struct args *args, char *value)
{
    int count = 0;
    vec_for_each2(char *, str, args->values)
    {
        if (strcmp(value, *str))
            count++;
    }

    return count;
}

bool
args_get_key_value(struct args *args, char *key, struct key_value *out_kv)
{
    vec_for_each2(struct key_value, kv, args->key_values)
    {
        if (strcmp(key, kv->key) == 0)
        {
            memcpy(out_kv, kv, sizeof(struct key_value));
            return true;
        }
    }

    return false;
}
