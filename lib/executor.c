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
    bool stopping;

    pthread_mutex_t *lock;
};

static void *execute(void *data);

static int process_start(char **command, char *work_dir, struct executor_command_env *env);
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
executor_signal_stop(struct executor *executor)
{
    log_info("(executor) Signal stopping\n");

    executor->stopping = true;

    return 0;
}

int
executor_run_commands(struct executor *executor, struct executor_command *commands)
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
executor_stop_commands_and_wait(struct executor *executor, bool force_quit)
{
    if (!executor->thr)
        return 1;

    pthread_mutex_lock(executor->lock);

    executor->stopping = true;

    vec_for_each2(struct executor_command, command, executor->commands)
    {
        if (command->pid && (!command->no_interrupt || force_quit))
            process_kill(command->pid);
    }

    pthread_mutex_unlock(executor->lock);

    pthread_join(executor->thr, NULL);

    vec_for_each2(struct executor_command, command, executor->commands)
    {
        vec_for_each(command->exec, free);
        vec_free(command->exec);

        vec_for_each2(struct executor_command_env, e, command->env)
        {
            free(e->key);
            free(e->value);

            e->key = NULL;
            e->value = NULL;
        }
        vec_free(command->env);

        free(command->name);
        free(command->work_dir);

        command->exec = NULL;
        command->env = NULL;
        command->name = NULL;
        command->work_dir = NULL;
    }
    vec_free(executor->commands);
    executor->commands = NULL;

    executor->stopping = false;
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

    return 0;
}

static void *
execute(void *data)
{
    struct executor *executor = data;

    pthread_mutex_lock(executor->lock);

    for (size_t i = 0; i < vec_length(executor->commands); i++)
    {
        struct executor_command *command = &executor->commands[i];

        if (executor->stopping)
            break;

        scoped char *command_desc = str_printf("%s (%ld/%ld)", command->name, i + 1, vec_length(executor->commands));

        log_info("(executor) Starting command: '%s'\n", command_desc);
        pid_t pid = process_start(command->exec, command->work_dir, command->env);
        command->pid = pid;

        pthread_mutex_unlock(executor->lock);

        int status_code = process_wait(pid);

        pthread_mutex_lock(executor->lock);
        command->pid = 0;

        if (executor->stopping && !command->no_interrupt)
            log_info("(executor) Command '%s' interupted\n", command_desc);
        else
        {
            if (status_code != 0)
                log_error("(executor) Command '%s' exited with error status code %d\n", command_desc, status_code);
            else
                log_info("(executor) Command '%s' completed successfully \n", command_desc);
        }

        if (status_code != 0 || executor->stopping)
            break;
    }

    pthread_mutex_unlock(executor->lock);

    return NULL;
}

static int
process_start(char **command, char *work_dir, struct executor_command_env *env)
{
    pid_t pid = fork();
    if (pid != 0)
        return pid;

    int current_pid = getpid();

    log_debug("(executor) Starting process '%s - %d'\n", command[0], current_pid);

    if (env != NULL)
    {
        vec_for_each2(struct executor_command_env, e, env)
        {
            if (e->no_override && getenv(e->key))
                continue;
            setenv(e->key, e->value, 1);
        }
    }

    if (work_dir)
        chdir(work_dir);
    execvp(command[0], command);

    log_critical("(executor) Unable to execute command '%s' with pid %d, aborting\n", command[0], current_pid);
    _exit(123);
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
