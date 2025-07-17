#ifndef EXECUTOR__H
#define EXECUTOR__H

#include <pthread.h>

struct executor;

struct executor_command
{
    char *name;
    char **exec;
    char *work_dir;
    bool no_interrupt; 

    pid_t pid;
};

struct executor *executor_create();

int executor_run_commands(struct executor *executor, struct executor_command *commands);
int executor_wait_for_commands_to_finish(struct executor *executor, bool force_quit);

int executor_free(struct executor *executor);

#endif
