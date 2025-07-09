#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <sys/wait.h>

#include "executor.h"

#include "utils/memory.h"
#include "utils/log.h"
#include "utils/vector.h"
#include "utils/time.h"

struct executor
{
    struct executor_command *commands;

    pthread_t thr;
    bool exiting;

    pthread_mutex_t *lock;
};

static void *execute(void *data);
static int process_start(char **command);
static int process_wait(pid_t pid);
static int process_kill(int pid);

struct executor *
executor_create()
{
    pthread_mutex_t *lock = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(lock, NULL) != 0)
    {
        log_critical("(executor) Unable to initialize lock");
        return NULL;
    }

    struct executor *executor = calloc(1, sizeof(struct executor));
    executor->lock = lock;

    return executor;
}

int
executor_start(struct executor *executor, struct executor_command *commands)
{
    if (executor->thr)
    {
        log_error("(executor) unable to start executor because it is already running");
        return 1;
    }

    executor->commands = commands;
    pthread_t executor_thr;
    pthread_create(&executor_thr, NULL, execute, (void *)executor);
    executor->thr = executor_thr;
    return 0;
}

int
executor_stop_join_clear(struct executor *executor)
{
    if (!executor->thr)
        return 1;

    pthread_mutex_lock(executor->lock);

    executor->exiting = true;

    vec_for_each2(struct executor_command, command, executor->commands)
    {
        process_kill(command->pid);
    }

    pthread_mutex_unlock(executor->lock);

    pthread_join(executor->thr, NULL);

    vec_for_each2(struct executor_command, command, executor->commands)
    {
        vec_for_each(command->exec, free);
        vec_free(command->exec);
        free(command->name);

        command->exec = NULL;
        command->name = NULL;
    }
    vec_free(executor->commands);
    executor->commands = NULL;

    executor->exiting = false;
    executor->thr = 0;

    return 0;
}

int
executor_free(struct executor *executor)
{
    if (executor->thr)
    {
        log_error("(executor) unable to free executor because it is running, you need to stop it first");
        return 1;
    }

    pthread_mutex_destroy(executor->lock);
    free(executor->lock);

    executor->lock = NULL;

    free(executor);
}

static void *
execute(void *data)
{
    struct executor *executor = data;

    pthread_mutex_lock(executor->lock);

    for (size_t i = 0; i < vec_length(executor->commands); i++)
    {
        struct executor_command *command = &executor->commands[i];

        if (executor->exiting)
        {
            pthread_mutex_unlock(executor->lock);
            return NULL;
        }

        scoped char *command_desc = str_printf("%s (%d/%d)", command->name, i + 1, vec_length(executor->commands));

        log_info("(executor) Starting command: '%s'\n", command_desc);
        pid_t pid = process_start(command->exec);
        command->pid = pid;

        pthread_mutex_unlock(executor->lock);

        int status_code = process_wait(pid);

        pthread_mutex_lock(executor->lock);
        command->pid = 0;

        if (executor->exiting)
        {
            pthread_mutex_unlock(executor->lock);
            log_info("(executor) Command '%s' interupted\n", command_desc);
            return NULL;
        }

        if (status_code != 0)
        {
            pthread_mutex_unlock(executor->lock);
            log_error("(executor) Command '%s' exited with error status code %d\n", command_desc, status_code);
            return NULL;
        }
        else
            log_info("(executor) Command '%s' completed successfully \n", command_desc);
    }

    pthread_mutex_unlock(executor->lock);

    return NULL;
}

static int
process_start(char **command)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        int current_pid = getpid();

        log_debug("Starting process '%s - %d'\n", command[0], current_pid);

        execvp(command[0], command);

        log_critical("Unable to execute command '%s' with pid %d, aborting\n", command[0], current_pid);
        _exit(123);
    }

    return pid;
}

static int
process_wait(pid_t pid)
{
    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

static int
process_kill(int pid)
{
    if (pid <= 0)
        return -1;

    int attempt = 0;
    while (attempt < 700 && kill(pid, 0) == 0)
    {
        if (attempt % 100 == 0)
            if (!kill(pid, attempt >= 500 ? SIGKILL : SIGTERM))
                break;

        if (attempt)
            sleep_ms(5);

        attempt++;
    };

    return attempt;
}
