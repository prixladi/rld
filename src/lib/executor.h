#ifndef EXECUTOR__H
#define EXECUTOR__H

#include <pthread.h>

struct executor;

struct executor_command
{
    char *name;
    char **exec;
    pid_t pid;
};

struct executor *executor_create();
int executor_stop_join_clear(struct executor *executor);
int executor_start(struct executor *executor, struct executor_command *commands);
int executor_free(struct executor *executor);

#endif
