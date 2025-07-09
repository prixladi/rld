#include <stdlib.h>

#include "lib/main.h"

#include "lib/utils/string.h"
#include "lib/utils/vector.h"

__MAIN

struct config
create_config(struct context *context)
{
    struct config config = { .watch_paths = vec_create_prealloc(char *, 2),
                             .build_command = vec_create_prealloc(char *, 3),
                             .run_command = vec_create_prealloc(char *, 3),
                             .debounce_ms = 500 };

    vec_push(config.watch_paths, "./run");
    vec_push(config.watch_paths, "./run2");

    // vec_push(config.build_command, "sleep");
    // vec_push(config.build_command, "4");
    // vec_push(config.build_command, NULL);

    // vec_push(config.run_command, "bash");
    // vec_push(config.run_command, "-c");
    // vec_push(config.run_command, "echo ahahahah && sleep 4 && echo AHAHAHAH");
    // vec_push(config.run_command, NULL);

    vec_push(config.build_command, "gcc");
    vec_push(config.build_command, "./run/main.c");
    vec_push(config.build_command, "-o");
    vec_push(config.build_command, "./run/run.out");
    vec_push(config.build_command, NULL);

    vec_push(config.run_command, "./run/run.out");
    vec_push(config.run_command, "reload");
    vec_push(config.run_command, NULL);

    return config;
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
    vec_free(config->build_command);
    vec_free(config->run_command);

    config->watch_paths = NULL;
    config->build_command = NULL;
    config->run_command = NULL;
}
