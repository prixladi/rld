#ifndef HELPERS__H
#define HELPERS__H

#include "interface.h"

bool file_has_extension(const char *file, const char *extension);
bool path_contains_subpath(const char *path, const char *_sp, bool from_start);

int args_contain_flag(struct args *args, char f);
int args_contain_long_flag(struct args *args, char *f);
int args_contain_value(struct args *args, char *value);
bool args_get_key_value(struct args *args, char *key, struct key_value *out_kv);

#endif
