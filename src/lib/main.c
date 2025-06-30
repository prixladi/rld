#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/inotify.h>

#include "utils/vector.h"
#include "utils/string.h"
#include "utils/log.h"

#include "main.h"
#include "watcher.h"

bool should_include_dir(char *dir)
{
    return str_starts_with(dir, "./run");
}

void *read_loop_thr(void *data)
{
    struct watcher *watcher = data;

    int res = 0;
    struct watcher_event_batch batch;
    while ((res = watcher_read_event_batch(watcher, &batch)) == 0)
    {
        printf("Batch - DIR CHANGED %d EVENT COUNT %d", batch.dir_structure_changed, vec_length(batch.file_events));
        vec_for_each2(struct watcher_file_event, event, batch.file_events)
        {
            printf(" %s ", event->file_name);
        }
        printf("\n");
        watcher_clear_event_batch(batch);
    }
}

struct watcher *watcher_g = NULL;

static void
graceful_stop_handler(int signal)
{
    (void)signal;
    // Intentionally not using 'log_*' or 'printf' because it uses non-async-signal-safe functions
    write(STDOUT_FILENO, "[SGN] Received terminate signal, stopping\n", 43);
    if (watcher_g)
        watcher_signal_stop(watcher_g);
}

int entrypoint(int argc, char **argv)
{
    log_init(DEBUG);

    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    signal(SIGTERM, graceful_stop_handler);
    signal(SIGINT, graceful_stop_handler);
    signal(SIGHUP, graceful_stop_handler);

    struct Context context = {};

    struct Config config = init_config(&context);

    printf("%d args %s\n", argc, config.build_command[0]);

    destroy_config(&config);

    vec_scoped char **root_dirs = vec_create_prealloc(char *, 2);
    vec_push(root_dirs, "./run");
    vec_push(root_dirs, "./run/i");

    struct watcher *watcher = watcher_create(root_dirs, &should_include_dir);
    watcher_g = watcher;

    watcher_start_watching(watcher);

    pthread_t thr;
    pthread_create(&thr, NULL, read_loop_thr, (void *)watcher);

    watcher_join(watcher);

    pthread_join(thr, NULL);

    watcher_g = NULL;
    watcher_free(watcher);

    return 0;
}