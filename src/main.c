#include "main.h"
#include "stdlib.h"

__MAIN

struct Config init_config(struct Context *context)
{
    struct Config config = {0};

    char **watch_paths = malloc(sizeof(char *) * 3);
    watch_paths[0] = "./tst/src1";
    watch_paths[1] = "./tst/src2";
    watch_paths[2] = NULL;
    config.watch_paths = watch_paths;

    char **build_command = malloc(sizeof(char *) * 3);
    build_command[0] = "gcc";
    build_command[1] = "main.c";
    build_command[2] = NULL;
    config.build_command = build_command;
    config.build_command_workdir = "./tst";

    char **run_command = malloc(sizeof(char *) * 2);
    build_command[0] = "./a.out";
    build_command[1] = NULL;
    config.run_command = run_command;
    config.run_command_workdir = "./tst";

    return config;
}

void destroy_config(struct Config *config)
{
    free(config->build_command);
    free(config->run_command);
    free(config->watch_paths);

    config->build_command = NULL;
    config->run_command = NULL;
    config->watch_paths = NULL;
}