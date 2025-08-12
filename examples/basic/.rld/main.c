#include <stdlib.h>

#include <rld/rld.h>
#include <rld/helpers.h>

#include <rld/utils/string.h>
#include <rld/utils/vector.h>

__RLD_MAIN

int
config_init(struct config *config, struct context *context)
{
    config->watch_paths = vec_create_prealloc(char *, 1);
    vec_push(config->watch_paths, ".");
    config->debounce_ms = 500;

    return 0;
}

void
config_free(struct config *config, struct context *context)
{
    vec_free(config->watch_paths);
}

struct command *
commands_create(struct changes_context *changes_context, struct context *context)
{
    struct command *commands = vec_create_prealloc(struct command, 5);

    char **build_exec = vec_create_prealloc(char *, 4);
    vec_push(build_exec, "cc");
    vec_push(build_exec, "main.c");
    vec_push(build_exec, "-o");
    vec_push(build_exec, "a.out");
    struct command build_command = { .name = "build", .exec = build_exec, .no_interrupt = false };
    vec_push(commands, build_command);

    char **run_exec = vec_create_prealloc(char *, 1);
    vec_push(run_exec, "./a.out");
    struct command run_command = { .name = "execute", .exec = run_exec, .no_interrupt = false };
    vec_push(commands, run_command);

    return commands;
}

void
commands_free(struct command *commands, struct context *context)
{
    vec_free(commands[0].exec);
    vec_free(commands[1].exec);
    vec_free(commands);
}

bool
should_include_dir(char *dir, struct context *context)
{
    return strncmp(dir, ".", 1) == 0;
}

bool
should_include_file_change(char *dir, char *file_name, struct context *context)
{
    return file_has_extension(file_name, "c");
}
