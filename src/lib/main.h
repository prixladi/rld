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

struct command
{
    char *name;
    char **exec;
};

struct config config_create(struct context *context);
void config_free(struct config *context);

struct command *commands_create(struct changes_context *changes_context, struct context *context);
void commands_free(struct command *commands, struct context *context);

bool should_include_dir(char *dir);
bool should_include_file_change(char *dir, char *file_name);

#endif
