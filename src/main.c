#include <stdlib.h>

#include "lib/interface.h"

#include "lib/utils/string.h"
#include "lib/utils/vector.h"

__MAIN

struct config
config_create(struct context *context)
{
    struct config config = {
        .watch_paths = vec_create_prealloc(char *, 2), .debounce_ms = 500, .work_dir = NULL, .user_data = NULL
    };

    vec_push(config.watch_paths, "./run");
    vec_push(config.watch_paths, "./run2");

    return config;
}

struct command *
commands_create(struct changes_context *changes_context, struct context *context)
{
    char **build_exec = vec_create_prealloc(char *, 4);

    vec_push(build_exec, "gcc");
    vec_push(build_exec, "./run/main.c");
    vec_push(build_exec, "-o");
    vec_push(build_exec, "./run/run.out");

    char **run_exec = vec_create_prealloc(char *, 2);

    vec_push(run_exec, "./run/run.out");
    vec_push(run_exec, "reload");

    struct command *commands = vec_create_prealloc(struct command, 5);

    struct command build_command = { .name = "build", .exec = build_exec };
    struct command run_command = { .name = "execute", .exec = run_exec };

    vec_push(commands, build_command);
    vec_push(commands, run_command);

    return commands;
}

void
commands_free(struct command *commands, struct context *context)
{
    vec_for_each2(struct command, command, commands)
    {
        vec_free(command->exec);
        command->exec = NULL;
    }

    vec_free(commands);
}

bool
should_include_dir(char *dir)
{
    return !str_starts_with(dir, "./run2");
}

bool
should_include_file_change(char *dir, char *file_name)
{
    return str_ends_with(file_name, ".c");
}

void
config_free(struct config *config)
{
    vec_free(config->watch_paths);
    config->watch_paths = NULL;
}
