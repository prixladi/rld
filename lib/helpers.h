#ifndef HELPERS__H
#define HELPERS__H

#include "rld.h"

// Checks whether the filename contains the provided extension
// Extension can be passed with or without a dot prefix, eg. "ts" == ".ts"
bool file_has_extension(const char *file, const char *extension);

// Checks if the path contains the provided subpath using common file system rules
// Flag `from_start` determines if we should treat `sp` as the beginning of the path
bool path_contains_subpath(const char *path, const char *sp, bool from_start);

// Counts the number of flags matching the character `f`
int args_count_flag(struct args *args, char f);

// Counts the number of long flags matching the string `f`
int args_count_long_flag(struct args *args, char *f);

// Counts the number of value arguments matching the string `f`
int args_count_value(struct args *args, char *value);

// Tries to get the value of the key-value argument
// Returns true if the argument was found, otherwise false
bool args_get_key_value(struct args *args, char *key, struct key_value *out_kv);

#endif
