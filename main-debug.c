#include <stdlib.h>

#include "lib/rld.h"
#include "lib/helpers.h"

#include "lib/utils/string.h"
#include "lib/utils/vector.h"

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

    struct command *commands = vec_create_prealloc(struct command, 2);

    char **build_exec = vec_create_prealloc(char *, 4);
    vec_push(build_exec, "gcc");
    vec_push(build_exec, "./main.c");
    vec_push(build_exec, "-o");
    vec_push(build_exec, "./run.out");
    struct command build_command = { .name = "build", .exec = build_exec, .work_dir = "./run" };
    vec_push(commands, build_command);

    char **run_exec = vec_create_prealloc(char *, 2);
    vec_push(run_exec, "./run.out");
    vec_push(run_exec, "rld");
    struct command_env *env = vec_create_prealloc(struct command_env, 1);
    struct command_env e = { .key = "env", .value = "debug", .no_override = true };
    vec_push(env, e);
    struct command run_command = {
        .name = "execute", .exec = run_exec, .work_dir = "./run", .no_interrupt = false, .env = env
    };
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
        vec_free(command->env);

        command->exec = NULL;
        command->env = NULL;
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
should_include_file_change(char *dir, char *file_name, struct context *context)
{
    (void)dir;
    (void)context;

    return file_has_extension(file_name, "c");
}

void
config_free(struct config *config, struct context *context)
{
    (void)context;

    vec_free(config->watch_paths);
    config->watch_paths = NULL;
}
