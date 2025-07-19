#ifndef INTERFACE__H
#define INTERFACE__H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define __MAIN \
    int main(int argc, char **argv) \
    { \
        app(argc, argv); \
    }
int app(int argc, char **argv);

struct changed_file
{
    char *file_name;
    char *dir;

    bool created;
    bool deleted;
    bool modified;
};

struct key_value
{
    char *key;
    char *value;
};

struct args
{
    char *flags;
    char **long_flags;
    char **values;
    struct key_value *key_values;
};

struct config
{
    char **watch_paths;
    char *work_dir;
    int debounce_ms;

    void *user_data;
};

struct context
{
    char *version;
    struct config config;
    struct args args;
};

struct changes_context
{
    struct changed_file *changed_files;
    bool dir_structure_changed;
    bool is_first_run;
};

struct command_env
{
    char *key;
    char *value;
    bool no_override;
};

struct command
{
    char *name;
    char **exec;
    char *work_dir;
    struct command_env *env;
    bool no_interrupt;
};

int config_init(struct context *context, struct config *config);
void config_free(struct config *config, struct context *context);

struct command *commands_create(struct changes_context *changes_context, struct context *context);
void commands_free(struct command *commands, struct context *context);

bool should_include_dir(char *dir, struct context *context);
bool should_include_file_change(char *dir, char *file_name, struct context *context);

#endif
