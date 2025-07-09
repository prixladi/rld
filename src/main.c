#include <stdlib.h>

#include "lib/main.h"

#include "lib/utils/string.h"
#include "lib/utils/vector.h"

__MAIN

struct config
create_config(struct context *context)
{
    struct config config = {
        .watch_paths = vec_create_prealloc(char *, 2), .debounce_ms = 500, .work_dir = NULL, .user_data = NULL
    };

    vec_push(config.watch_paths, "./run");
    vec_push(config.watch_paths, "./run2");

    return config;
}

char **
get_build_command(struct changes_context *changes_context, struct context *context)
{
    char **command = vec_create_prealloc(char *, 5);

    vec_push(command, "gcc");
    vec_push(command, "./run/main.c");
    vec_push(command, "-o");
    vec_push(command, "./run/run.out");
    vec_push(command, NULL);

    return command;
}

void
free_build_command(char **command, struct context *context)
{
    vec_free(command);
}

char **
get_run_command(struct changes_context *changes_context, struct context *context)
{
    char **command = vec_create_prealloc(char *, 5);

    vec_push(command, "./run/run.out");
    vec_push(command, "reload");
    vec_push(command, NULL);

    return command;
}

void
free_run_command(char **command, struct context *context)
{
    vec_free(command);
}

bool
should_include_dir(char *dir)
{
    return str_starts_with(dir, "./run");
}

bool
should_include_file_change(struct changed_file *cf)
{
    return str_ends_with(cf->file_name, ".c");
}

void
free_config(struct config *config)
{
    vec_free(config->watch_paths);
    config->watch_paths = NULL;
}
