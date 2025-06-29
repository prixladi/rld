#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/inotify.h>

#include "utils/vector.h"
#include "utils/string.h"

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
        printf("Batch - %d %d \n", batch.dir_structure_changed, vec_length(batch.file_events));
        vec_for_each2(struct watcher_file_event, event, batch.file_events)
        {
            printf(" %s \n", event->file_name);
        }
        printf("\n");
        watcher_clear_event_batch(batch);
    }
    printf("CCCC\n");
}

int entrypoint(int argc, char **argv)
{
    struct Context context = {};

    struct Config config = init_config(&context);

    printf("%d args %s\n", argc, config.build_command[0]);

    destroy_config(&config);

    vec_scoped char **root_dirs = vec_create_prealloc(char *, 2);
    vec_push(root_dirs, "./run");
    vec_push(root_dirs, "./run/i");

    struct watcher *watcher = watcher_create(root_dirs, &should_include_dir);

    printf("staring\n");
    watcher_start_watching(watcher);

    sleep(10);

    pthread_t thr;
    pthread_create(&thr, NULL, read_loop_thr, (void *)watcher);

    sleep(5);

    printf("stoping\n");
    watcher_wait_and_stop_watching(watcher);
    pthread_join(thr, NULL);

    printf("stopped\n");

    watcher_free(watcher);

    return 0;
}