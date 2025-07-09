#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/select.h>

#include "utils/fs.h"
#include "utils/vector.h"
#include "utils/memory.h"
#include "utils/string.h"
#include "utils/log.h"
#include "utils/hashmap.h"

#include "watcher.h"

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

struct watched_dir
{
    char *dir;
    int wd;
    int notify_fd;
};

struct watcher
{
    char **root_dirs;
    bool (*should_include_dir)(char *);

    pthread_t watching_thr;
    bool stoped;

    struct hashmap *watched_dirs;
    struct watcher_event_batch event_batch;
    pthread_mutex_t *lock;
    pthread_cond_t *cond;
};

static void init_and_add_watched_dirs(struct watcher *watcher, int fd);
static void *watcher_start_watching_thr(void *data);

static void free_watched_dir(void *item);
static void free_pointer_to_string(void *item);

static int str_compare(const void *a, const void *b, void *udata);
static uint64_t str_hash(const void *item, uint64_t seed0, uint64_t seed1);
static int watched_dir_compare(const void *a, const void *b, void *udata);
static uint64_t watched_dir_hash(const void *item, uint64_t seed0, uint64_t seed1);

struct watcher *
watcher_create(char **root_dirs, bool (*should_include_dir)(char *))
{
    struct watcher *watcher = calloc(1, sizeof(struct watcher));

    char **r_dirs = vec_create_prealloc(char *, vec_length(root_dirs));
    vec_for_each2(char *, dir, root_dirs) vec_push(r_dirs, str_dup(*dir));

    watcher->stoped = true;
    watcher->root_dirs = r_dirs;
    watcher->should_include_dir = should_include_dir;

    watcher->watched_dirs =
        hashmap_new(sizeof(struct watched_dir), 0, 0, 0, str_hash, str_compare, &free_watched_dir, NULL);

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

int
watcher_free(struct watcher *watcher)
{
    vec_for_each(watcher->root_dirs, free);
    vec_free(watcher->root_dirs);

    hashmap_free(watcher->watched_dirs);

    watcher_clear_event_batch(watcher->event_batch);

    pthread_cond_destroy(watcher->cond);
    free(watcher->cond);

    pthread_mutex_destroy(watcher->lock);
    free(watcher->lock);

    watcher->should_include_dir = NULL;

    watcher->root_dirs = NULL;

    watcher->watched_dirs = NULL;

    watcher->lock = NULL;
    watcher->cond = NULL;

    free(watcher);
}

int
watcher_start_watching(struct watcher *watcher)
{
    log_info("(watcher) Starting\n");

    pthread_t thr;
    watcher->stoped = false;
    pthread_create(&thr, NULL, watcher_start_watching_thr, (void *)watcher);
    watcher->watching_thr = thr;

    return 0;
}

int
watcher_signal_stop(struct watcher *watcher)
{
    log_info("(watcher) Stopping\n");

    watcher->stoped = true;
    pthread_cond_broadcast(watcher->cond);
}

int
watcher_join(struct watcher *watcher)
{
    if (watcher->watching_thr <= 0)
        return 1;

    pthread_cond_broadcast(watcher->cond);
    pthread_join(watcher->watching_thr, NULL);

    watcher->watching_thr = -1;

    return 0;
}

int
watcher_read_event_batch(struct watcher *watcher, struct watcher_event_batch *batch)
{
    for (; !watcher->stoped;)
    {
        pthread_mutex_lock(watcher->lock);

        bool batch_is_empty = !watcher->event_batch.dir_structure_changed &&
                              !vec_length(watcher->event_batch.file_events);
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

int
watcher_clear_event_batch(struct watcher_event_batch batch)
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
    int notify_fd = inotify_init();

    init_and_add_watched_dirs(watcher, notify_fd);

    char buffer[BUF_LEN];
    for (; !watcher->stoped;)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(notify_fd, &read_fds);

        struct timeval timeout = { .tv_sec = 0, .tv_usec = 1000 * 100 };

        // TODO: Use pselect or other fd for instant interruption, this causes delay of the timeout duration on exit
        int select_status = select(notify_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (select_status > 0 && FD_ISSET(notify_fd, &read_fds) && !watcher->stoped)
        {
            int length = read(notify_fd, buffer, BUF_LEN);

            bool dir_action = false;
            vec_scoped struct watcher_file_event *file_events = vec_create(struct watcher_file_event);

            for (int i = 0; i < length;)
            {
                struct inotify_event *event = (struct inotify_event *)&buffer[i];
                i += EVENT_SIZE + event->len;

                if (event->mask & (IN_ISDIR | IN_DELETE_SELF | IN_MOVE_SELF))
                {
                    dir_action = true;
                    continue;
                }

                if (event->mask & IN_IGNORED)
                {
                    continue;
                }

                size_t iter_wd = 0;
                void *item_wd;
                char *event_dir = NULL;
                while (hashmap_iter(watcher->watched_dirs, &iter_wd, &item_wd))
                {
                    const struct watched_dir *w_dir = item_wd;
                    if (w_dir->wd == event->wd)
                        event_dir = w_dir->dir;
                }

                if (!event_dir)
                {
                    log_critical("(watcher) IO Event occurred on wd '%d' that is not included in list of active watchers\n",
                                 event->wd);
                    continue;
                }

                struct watcher_file_event file_event = { .dir = str_dup(event_dir),
                                                         .file_name = str_dup(event->name),
                                                         .created = event->mask & IN_CREATE,
                                                         .deleted = event->mask & IN_DELETE,
                                                         .modified = event->mask & IN_CLOSE_WRITE,
                                                         .moved_from = event->mask & IN_MOVED_FROM,
                                                         .moved_to = event->mask & IN_MOVED_TO,
                                                         .timestamp = time(NULL) };

                vec_push(file_events, file_event);
            }

            if (dir_action)
                init_and_add_watched_dirs(watcher, notify_fd);

            pthread_mutex_lock(watcher->lock);

            if (dir_action)
                watcher->event_batch.dir_structure_changed = true;
            if (vec_length(file_events))
                vec_push_many(watcher->event_batch.file_events, file_events);

            pthread_mutex_unlock(watcher->lock);
            pthread_cond_broadcast(watcher->cond);
        }
    }

    hashmap_clear(watcher->watched_dirs, 0);
    close(notify_fd);

    return 0;
}

static void
init_and_add_watched_dirs(struct watcher *watcher, int notify_fd)
{
    struct hashmap *scanned_dirs_map =
        hashmap_new(sizeof(char *), 0, 0, 0, str_hash, str_compare, &free_pointer_to_string, NULL);
    vec_for_each2(char *, r_dir, watcher->root_dirs)
    {
        vec_scoped char **dirs = get_directories_recursive(*r_dir);
        vec_for_each2(char *, dir_p, dirs)
        {
            char *dir = *dir_p;
            if (watcher->should_include_dir && !watcher->should_include_dir(dir))
            {
                free(dir);
                continue;
            }

            char **replaced = (char **)hashmap_set(scanned_dirs_map, &dir);
            if (replaced)
                free(*replaced);
        }
    }

    size_t iter_wd = 0;
    void *item_wd;
    while (hashmap_iter(watcher->watched_dirs, &iter_wd, &item_wd))
    {
        const struct watched_dir *w_dir = item_wd;
        if (!hashmap_get(scanned_dirs_map, &w_dir->dir))
        {
            // TODO: this is not ideal, find other way to iterate it without having to reset the cursor after each delete
            iter_wd = 0;
            // We need to copy here because after hashmap_delete pointer moves
            struct watched_dir w_dir_cp = *w_dir;
            hashmap_delete(watcher->watched_dirs, &w_dir_cp);

            inotify_rm_watch(w_dir->notify_fd, w_dir_cp.wd);
            free_watched_dir((void *)&w_dir_cp);
        }
    }

    size_t iter_sd = 0;
    void *item_sd;
    while (hashmap_iter(scanned_dirs_map, &iter_sd, &item_sd))
    {
        char **dir = item_sd;
        struct watched_dir search_dir = { .dir = *dir };

        if (!hashmap_get(watcher->watched_dirs, &search_dir))
        {
            unsigned int eventMask = IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVE | IN_DELETE_SELF | IN_MOVE_SELF;
            int wd = inotify_add_watch(notify_fd, *dir, eventMask);

            struct watched_dir wtd;
            wtd.dir = str_dup(*dir);
            wtd.wd = wd;
            wtd.notify_fd = notify_fd;

            hashmap_set(watcher->watched_dirs, &wtd);
        }
    }

    hashmap_free(scanned_dirs_map);

    log_debug("(watcher) Now watching %d dirs\n", hashmap_count(watcher->watched_dirs));
}

static void
free_watched_dir(void *item)
{
    struct watched_dir *w_dir = item;
    free(w_dir->dir);
}

static void
free_pointer_to_string(void *item)
{
    char **str = item;
    free(*str);
}

static int
str_compare(const void *a, const void *b, void *udata)
{
    const char *const *sa = a;
    const char *const *sb = b;
    return strcmp(*sa, *sb);
}

static uint64_t
str_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const char *const *str = item;
    return hashmap_sip(*str, strlen(*str), seed0, seed1);
}

static int
watched_dir_compare(const void *a, const void *b, void *udata)
{
    const struct watched_dir *wa = a;
    const struct watched_dir *wb = b;
    return strcmp(wa->dir, wb->dir);
}

static uint64_t
watched_dir_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const struct watched_dir *dir = item;
    return hashmap_sip(dir->dir, strlen(dir->dir), seed0, seed1);
}
