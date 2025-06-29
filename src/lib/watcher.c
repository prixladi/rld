#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/inotify.h>

#include "utils/fs.h"
#include "utils/vector.h"
#include "utils/memory.h"
#include "utils/string.h"
#include "utils/log.h"

#include "watcher.h"

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

struct watched_dir
{
    char *dir;
    int wd;
    int watcher_fd;
};

struct watcher
{
    char **root_dirs;
    bool (*should_include_dir)(char *);

    pthread_t watching_thr;
    bool stoped;

    struct watcher_event_batch event_batch;
    pthread_mutex_t *lock;
    pthread_cond_t *cond;
};

static void init_and_add_watchers(struct watcher *watcher, struct watched_dir **watched_dirs, char **dirs, int fd);
static void rm_and_free_watchers(struct watched_dir **watched_dirs);
static void *watcher_start_watching_thr(void *data);

struct watcher *
watcher_create(char **root_dirs, bool (*should_include_dir)(char *))
{
    struct watcher *watcher = calloc(1, sizeof(struct watcher));

    char **r_dirs = vec_create_prealloc(char *, vec_length(root_dirs));
    vec_for_each2(char *, dir, root_dirs)
    {
        char *duplicate = str_dup(*dir);
        vec_push(r_dirs, duplicate);
    }
    watcher->stoped = true;
    watcher->root_dirs = r_dirs;
    watcher->should_include_dir = should_include_dir;
    watcher->event_batch.dir_structure_changed = false;
    watcher->event_batch.file_events = vec_create(struct watcher_file_event);

    pthread_mutex_t *lock = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(lock, NULL) != 0)
    {
        free(lock);
        log_critical("Unable to initialize watcher lock");
        return NULL;
    }

    pthread_cond_t *cond = malloc(sizeof(pthread_cond_t));
    if (pthread_cond_init(cond, NULL) != 0)
    {
        free(lock);
        free(cond);
        log_critical("Unable to initialize watcher cond\n");
        return NULL;
    }

    watcher->lock = lock;
    watcher->cond = cond;

    return watcher;
}

int watcher_free(struct watcher *watcher)
{
    vec_for_each(watcher->root_dirs, free);
    vec_free(watcher->root_dirs);
    watcher_clear_event_batch(watcher->event_batch);

    pthread_cond_destroy(watcher->cond);
    free(watcher->cond);

    pthread_mutex_destroy(watcher->lock);
    free(watcher->lock);

    watcher->should_include_dir = NULL;
    watcher->root_dirs = NULL;

    watcher->lock = NULL;
    watcher->cond = NULL;

    free(watcher);
}

int watcher_start_watching(struct watcher *watcher)
{
    pthread_t thr;
    watcher->stoped = false;
    pthread_create(&thr, NULL, watcher_start_watching_thr, (void *)watcher);
    watcher->watching_thr = thr;

    return 0;
}

int watcher_wait_and_stop_watching(struct watcher *watcher)
{
    if (watcher->watching_thr <= 0)
        return 1;

    watcher->stoped = true;
    pthread_join(watcher->watching_thr, NULL);

    watcher->watching_thr = -1;

    return 0;
}

int watcher_read_event_batch(struct watcher *watcher, struct watcher_event_batch *batch)
{
    for (; !watcher->stoped;)
    {
        pthread_mutex_lock(watcher->lock);

        bool batch_is_empty = !watcher->event_batch.dir_structure_changed && !vec_length(watcher->event_batch.file_events);
        if (!batch_is_empty)
        {
            memcpy(batch, &watcher->event_batch, sizeof(struct watcher_event_batch));

            watcher->event_batch.dir_structure_changed = false;
            watcher->event_batch.file_events = vec_create(struct watcher_file_event);
        }

        if (batch_is_empty)
            pthread_cond_wait(watcher->cond, watcher->lock);

        pthread_mutex_unlock(watcher->lock);

        if (!batch_is_empty)
            break;
    }

    return watcher->stoped ? -1 : 0;
}

int watcher_clear_event_batch(struct watcher_event_batch batch)
{
    vec_for_each2(struct watcher_file_event, event, batch.file_events)
    {
        free(event->file_name);
        free(event->dir);

        event->file_name = NULL;
        event->dir = NULL;
    }

    vec_free(batch.file_events);

    batch.file_events = NULL;
    batch.dir_structure_changed = false;
}

static void *
watcher_start_watching_thr(void *data)
{
    struct watcher *watcher = data;
    int fd = inotify_init();

    vec_scoped struct watched_dir *watched_dirs = vec_create_prealloc(struct watched_dir, 8);
    init_and_add_watchers(watcher, &watched_dirs, watcher->root_dirs, fd);

    char buffer[BUF_LEN];
    for (; !watcher->stoped;)
    {
        // printf("---------------\n");
        // vec_for_each2(struct watched_dir, watched_dir, watched_dirs)
        //     printf("%d - %s\n", watched_dir->wd, watched_dir->dir);
        // printf("---------------\n");

        int length = read(fd, buffer, BUF_LEN);

        bool dir_action = false;
        vec_scoped struct watcher_file_event *file_events = vec_create(struct watcher_file_event);

        for (int i = 0; i < length;)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            i += EVENT_SIZE + event->len;

            if (event->mask & IN_ISDIR)
            {
                dir_action = true;
                continue;
            }

            char *event_dir = NULL;
            vec_for_each2(struct watched_dir, watched_dir, watched_dirs)
            {
                if (watched_dir->wd == event->wd)
                {
                    event_dir = watched_dir->dir;
                    break;
                }
            }

            if (!event_dir)
            {
                log_critical("IO Event occurred on wd '%d' that is not included in list of active watchers", event->wd);
                continue;
            }

            struct watcher_file_event file_event = {
                .dir = str_dup(event_dir),
                .file_name = str_dup(event->name),
                .created = event->mask & IN_CREATE,
                .modified = event->mask & IN_CLOSE_WRITE,
                .moved_from = event->mask & IN_MOVED_FROM,
                .moved_to = event->mask & IN_MOVED_TO,
                .timestamp = time(NULL)};

            vec_push(file_events, file_event);
        }

        if (dir_action)
        {
            rm_and_free_watchers(&watched_dirs);
            init_and_add_watchers(watcher, &watched_dirs, watcher->root_dirs, fd);
        }

        pthread_mutex_lock(watcher->lock);

        if (dir_action)
            watcher->event_batch.dir_structure_changed = true;
        if (vec_length(file_events))
            vec_push_many(watcher->event_batch.file_events, file_events);

        pthread_mutex_unlock(watcher->lock);
        pthread_cond_broadcast(watcher->cond);
    }

    rm_and_free_watchers(&watched_dirs);
    close(fd);

    return 0;
}

static void
init_and_add_watchers(struct watcher *watcher, struct watched_dir **watched_dirs, char **root_dirs, int fd)
{
    vec_for_each2(char *, r_dir, watcher->root_dirs)
    {
        vec_scoped char **dirs = get_directories_recursive(str_dup(*r_dir));

        for (size_t i = 0; i < vec_length(dirs); i++)
        {
            char *dir = dirs[i];
            if (watcher->should_include_dir && !watcher->should_include_dir(dir))
                continue;

            // TODO: Idk if it is necessary to check for duplicate directories
            //       inotify_add_watch is smart and reuses wd for duplicate dirs
            //       this just means that we might have trash in watched_dirs array
            //       if it becomes a problem code bellow should do the trick
            //
            // bool already_exists = false;
            // vec_for_each2(struct watched_dir, w_dir, *watched_dirs)
            // {
            //     if (strncmp(dir, w_dir->dir, strlen(dir)) == 0)
            //     {
            //         already_exists = true;
            //         break;
            //     }
            // }

            // if (already_exists)
            //     continue;

            int wd = inotify_add_watch(fd, dir, IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVE | IN_DELETE_SELF | IN_MOVE_SELF);
            struct watched_dir wtd = {
                .dir = dir,
                .wd = wd,
                .watcher_fd = fd};

            vec_push(*watched_dirs, wtd);
        }
    }
}

static void
rm_and_free_watchers(struct watched_dir **watched_dirs)
{
    struct watched_dir w_dir;
    while (!vec_pop(*watched_dirs, &w_dir))
    {
        inotify_rm_watch(w_dir.watcher_fd, w_dir.wd);
        free(w_dir.dir);
    }
}
