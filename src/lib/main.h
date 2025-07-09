#ifndef MAIN__H
#define MAIN__H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define __MAIN \
    int main(int argc, char **argv) \
    { \
        entrypoint(argc, argv); \
    }
int entrypoint(int argc, char **argv);

struct changed_file
{
    char *file_name;
    char *dir;

    bool created;
    bool deleted;
    bool modified;
};

struct context
{
};

struct config
{
    char **watch_paths;
    char *work_dir;
    int debounce_ms;

    void *user_data;

    char **build_command;
    char **run_command;
};

struct config create_config(struct context *context);
void free_config(struct config *context);

bool should_include_dir(char *dir);
bool should_include_file_change(struct changed_file *cf);

#endif
