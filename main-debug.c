#include <stdlib.h>

#include "src/lib/rld.h"
#include "src/lib/helpers.h"

#include "src/lib/utils/string.h"
#include "src/lib/utils/vector.h"

__MAIN

int
config_init(struct context *context, struct config *config)
{
    (void)context;

    config->watch_paths = vec_create_prealloc(char *, 2);
    config->debounce_ms = 500;
    config->work_dir = NULL;
    config->user_data = NULL;

    vec_push(config->watch_paths, "run");
    vec_push(config->watch_paths, "run2");

    return 0;
}

struct command *
commands_create(struct changes_context *changes_context, struct context *context)
{
    (void)changes_context;
    (void)context;

    char **build_exec = vec_create_prealloc(char *, 4);

    vec_push(build_exec, "gcc");
    vec_push(build_exec, "./main.c");
    vec_push(build_exec, "-o");
    vec_push(build_exec, "./run.out");

    char **run_exec = vec_create_prealloc(char *, 2);

    vec_push(run_exec, "./run.out");
    vec_push(run_exec, "rld");

    struct command *commands = vec_create_prealloc(struct command, 5);

    struct command build_command = { .name = "build", .exec = build_exec, .work_dir = "./run", 0 };
    struct command run_command = { .name = "execute", .exec = run_exec, .work_dir = "./run", .no_interrupt = false };

    vec_push(commands, build_command);
    vec_push(commands, run_command);

    return commands;
}

void
commands_free(struct command *commands, struct context *context)
{
    (void)context;

    vec_for_each2(struct command, command, commands)
    {
        vec_free(command->exec);
        command->exec = NULL;
    }

    vec_free(commands);
}

bool
should_include_dir(char *dir, struct context *context)
{
    (void)context;
    
    return !path_contains_subpath(dir, "run2", true);
}

bool
should_include_file_change(char *dir ,char *file_name, struct context *context)
{
    (void)dir;
    (void)context;

    return file_has_extension(file_name, "c");
}

void
config_free(struct config* config, struct context *context)
{
    (void)context;

    vec_free(config->watch_paths);
    config->watch_paths = NULL;
}
