#ifndef EXECUTOR__H
#define EXECUTOR__H

#include <pthread.h>

#include "rld.h"

struct executor;

struct executor *executor_create();

int executor_run_commands(struct executor *executor, struct command *commands_src);
int executor_signal_stop(struct executor *executor);
struct command *executor_stop_commands_and_wait(struct executor *executor, bool force_quit);
int executor_free(struct executor *executor);

#endif
