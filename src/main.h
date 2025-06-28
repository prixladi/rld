#include <stdio.h>

#include <sys/types.h>
#include <sys/inotify.h>

#define __MAIN \
    int main(int argc, char **argv) { entrypoint(argc, argv); }

struct Context
{
};

struct Config
{
    char **watch_paths;

    char **build_command;
    char *build_command_workdir;

    char **run_command;
    char *run_command_workdir;

    void *user_data;
};

struct ChangedField
{
    char *path;
};

struct Config init_config(struct Context *context);
void destroy_config(struct Config *context);

static int entrypoint(int argc, char **argv)
{
    struct Context context = {};

    struct Config config = init_config(&context);

    printf("%d args %s\n", argc, config.build_command[0]);

    destroy_config(&config);
}