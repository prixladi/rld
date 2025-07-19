#ifndef EXECUTOR__H
#define EXECUTOR__H

#include <pthread.h>

struct executor;

struct executor_command_env
{
    char *key;
    char *value;
    bool no_override;
};

struct executor_command
{
    char *name;
    char **exec;
    char *work_dir;
    struct executor_command_env *env;
    bool no_interrupt;

    pid_t pid;
};

struct executor *executor_create();

int executor_run_commands(struct executor *executor, struct executor_command *commands);
int executor_signal_stop(struct executor *executor);
int executor_stop_commands_and_wait(struct executor *executor, bool force_quit);
int executor_free(struct executor *executor);

#endif
