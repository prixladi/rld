#ifndef EXECUTOR__H
#define EXECUTOR__H

#include <pthread.h>

struct executor;

struct executor_command
{
    char *name;
    char **exec;
    char *work_dir;
    pid_t pid;
};

struct executor *executor_create();

int executor_run_commands(struct executor *executor, struct executor_command *commands);
int executor_stop_commands_and_wait(struct executor *executor);

int executor_free(struct executor *executor);

#endif
