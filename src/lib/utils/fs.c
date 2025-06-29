#include <stdbool.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include "string.h"
#include "vector.h"
#include "fs.h"

#define DT_DIR 4

bool dir_exists(char *path)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
        return false;

    closedir(dir);
    return true;
}

char *
get_file_content(FILE *fp)
{
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *content = malloc(size * sizeof(char) + 1);
    fread(content, sizeof(char), size, fp);

    content[size] = '\0';

    return content;
}

bool is_path_absolute(char *path)
{
    return strlen(path) > 0 && path[0] == '/';
}

char *
paths_join(char *p1, char *p2)
{
    if (p1 == NULL || strlen(p1) == 0)
        return p2 == NULL ? NULL : str_dup(p2);

    if (p2 == NULL || strlen(p2) == 0)
        return p1 == NULL ? NULL : str_dup(p1);

    if (p1[strlen(p1) - 1] == '/')
        return str_concat(p1, p2);

    return str_printf("%s/%s", p1, p2);
}

char **
get_directories_recursive(char *root_dir)
{
    char **dirs = vec_create_prealloc(char *, 8);

    vec_scoped char **stack = vec_create_prealloc(char *, 8);
    vec_push(stack, root_dir);

    do
    {
        char *current_dir;
        vec_pop(stack, &current_dir);

        DIR *dir = opendir(current_dir);
        if (dir == NULL)
            continue;

        vec_push(dirs, current_dir);

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strncmp(entry->d_name, ".", 1) == 0 || strncmp(entry->d_name, "..", 2) == 0)
                continue;

            if (entry->d_type != DT_DIR)
                continue;
            char *dir_path = paths_join(current_dir, entry->d_name);

            vec_push(stack, dir_path);
        }

        closedir(dir);
    } while (vec_length(stack));

    return dirs;
}