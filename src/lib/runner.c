#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utils/log.h"
#include "utils/time.h"

#include "runner.h"

int
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

int
process_wait(pid_t pid)
{
    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

int
process_kill(int pid)
{
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
