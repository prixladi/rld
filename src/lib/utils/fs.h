#ifndef FS__H
#define FS__H

#include <stdbool.h>
#include <stdio.h>

bool dir_exists(const char *path);
char *get_file_content(FILE *fp);
bool is_path_absolute(const char *path);
char *paths_join(const char *p1, const char *p2);

#endif
