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

struct Context
{
};

struct Config
{
    char **watch_paths;

    char **build_command;
    char *build_command_workdir;

    char **run_command;
    char *run_command_workdir;

    void *user_data;
};

struct ChangedFile
{
    char *path;
    uint32_t changes_mask;
};

struct Config init_config(struct Context *context);
bool should_register_file_change(struct ChangedFile changed_file);
void destroy_config(struct Config *context);

#endif
