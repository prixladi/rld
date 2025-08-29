#include <stdlib.h>

#include <rld/rld.h>
#include <rld/helpers.h>

#include <rld/utils/string.h>
#include <rld/utils/vector.h>
#include <rld/utils/log.h>

struct service
{
    char *name;
    char **deps;
};

struct run_config
{
    struct service *service;
};

static struct service services[] = {
    { .name = "service1", .deps = (char *[]){ NULL } },
    { .name = "service2", .deps = (char *[]){ "package1", NULL } },
    { .name = "service3", .deps = (char *[]){ "package1", "package2", NULL } },
    { .name = NULL, .deps = NULL },
};

__RLD_MAIN

int
config_init(struct config *config, struct context *context)
{
    struct run_config *rc = calloc(1, sizeof(struct run_config));

    if (vec_length(context->args.values) != 1)
    {
        log_critical("Command requires exactly one service argument.\n");
        free(rc);
        return 1;
    }

    char *arg_service = context->args.values[0];

    for (struct service *service = services; service->name != NULL; service++)
    {
        if (strcmp(service->name, arg_service) == 0)
        {
            rc->service = service;
            break;
        }
    }

    if (rc->service == NULL)
    {
        log_critical("Service '%s' is not defined in config.\n", arg_service);
        free(rc);
        return 1;
    }

    config->watch_paths = vec_create_prealloc(char *, 1);
    vec_push(config->watch_paths, str_printf("./services/%s", rc->service->name));

    for (char **dep = rc->service->deps; *dep != NULL; dep++)
        vec_push(config->watch_paths, str_printf("./packages/%s", *dep));

    config->debounce_ms = 500;
    config->user_data = rc;

    return 0;
}

void
config_free(struct config *config, struct context *context)
{
    vec_for_each(config->watch_paths, free);
    vec_free(config->watch_paths);

    free(config->user_data);
    config->user_data = NULL;
}

struct command *
commands_create(struct changes_context *changes_context, struct context *context)
{
    struct run_config *rc = context->config.user_data;

    struct command *commands = vec_create_prealloc(struct command, 5);

    for (char **dep = rc->service->deps; *dep != NULL; dep++)
    {
        char **build_exec = vec_create_prealloc(char *, 7);
        vec_push(build_exec, "yarn");
        vec_push(build_exec, "build");
        struct command build_command = { .name = str_concat("build ", *dep),
                                         .exec = build_exec,
                                         .work_dir = str_concat("./packages/", *dep),
                                         .no_interrupt = false };
        vec_push(commands, build_command);
    }

    char **build_exec = vec_create_prealloc(char *, 7);
    vec_push(build_exec, "yarn");
    vec_push(build_exec, "build");
    struct command build_command = { .name = str_concat("build ", rc->service->name),
                                     .exec = build_exec,
                                     .work_dir = str_concat("./services/", rc->service->name),
                                     .no_interrupt = false };
    vec_push(commands, build_command);

    char **run_exec = vec_create_prealloc(char *, 2);
    vec_push(run_exec, "yarn");
    vec_push(run_exec, "start");
    struct command run_command = { .name = str_concat("execute ", rc->service->name),
                                   .exec = run_exec,
                                   .work_dir = str_concat("./services/", rc->service->name),
                                   .no_interrupt = false,
                                   .env = NULL };
    vec_push(commands, run_command);

    return commands;
}

void
commands_free(struct command *commands, struct context *context)
{
    vec_for_each2(struct command, command, commands)
    {
        free(command->work_dir);
        free(command->name);
        vec_free(command->exec);
    }
    vec_free(commands);
}

bool
should_include_dir(char *dir, struct context *context)
{
    return !path_contains_subpath(dir, "node_modules", false) && !path_contains_subpath(dir, "out", false);
}

bool
should_include_file_change(char *dir, char *file_name, struct context *context)
{
    return file_has_extension(file_name, ".js") || file_has_extension(file_name, ".ts");
}

bool
print_usage(const char *app_name)
{
    printf("Usage: %s <service> [options]...\n\
Rld monorepo example\n\
Services: service1, service2, service3\n\
Options:\n\
    -h, --help          Prints help\n\
    -v,                 Prints verbose logs, the more arguments are provided the more verbose they get\n\
Examples\n\
    %s service1\n\
",
           app_name, app_name);

    return true;
}
