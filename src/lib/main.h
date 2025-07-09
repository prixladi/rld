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

struct changes_context
{
    struct changed_file *changed_files;
    bool dir_structure_changed;
    bool is_first_run;
};

struct config
{
    char **watch_paths;
    char *work_dir;
    int debounce_ms;

    void *user_data;
};

struct config create_config(struct context *context);
void free_config(struct config *context);

char **get_build_command(struct changes_context *changes_context, struct context *context);
void free_build_command(char **command, struct context *context);

char **get_run_command(struct changes_context *changes_context, struct context *context);
void free_run_command(char **command, struct context *context);

bool should_include_dir(char *dir);
bool should_include_file_change(struct changed_file *cf);

#endif
