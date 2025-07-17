#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/select.h>

#include "utils/vector.h"
#include "utils/memory.h"
#include "utils/string.h"
#include "utils/log.h"
#include "utils/hashmap.h"
#include "utils/fs.h"
#include "utils/time.h"

#include "watcher.h"

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

#define WATCHED_EVENT_MASK (IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVE | IN_DELETE_SELF | IN_MOVE_SELF)
#define DIR_EVENT_MASK (IN_ISDIR | IN_DELETE_SELF | IN_MOVE_SELF)

struct watched_dir
{
    char *dir;
    int wd;
    int notify_fd;
};

struct watcher
{
    char **root_dirs;
    bool (*should_include_dir)(char *, void *);
    bool (*should_include_file_change)(char *, char *, void *);

    pthread_t thr;
    bool stoped;

    struct hashmap *watched_dirs;
    struct watcher_event_batch event_batch;
    pthread_mutex_t *lock;
    pthread_cond_t *cond;

    void *context;
};

static void init_and_add_watched_dirs(struct watcher *watcher, int fd);
static void *watcher_start_watching_thr(void *data);

static void watched_dir_free(void *item);
static void pointer_to_string_free(void *item);

static int str_compare(const void *a, const void *b, void *udata);
static uint64_t str_hash(const void *item, uint64_t seed0, uint64_t seed1);

static char **get_directories_recursive(const char *root_dir, bool (*should_include_dir)(char *, void *), void *context);

struct watcher *
watcher_create(char **root_dirs, bool (*should_include_dir)(char *, void *),
               bool (*should_include_file_change)(char *, char *, void *), void *context)
{
    struct watcher *watcher = calloc(1, sizeof(struct watcher));

    char **r_dirs = vec_create_prealloc(char *, vec_length(root_dirs));
    vec_for_each2(char *, dir, root_dirs) vec_push(r_dirs, str_dup(*dir));

    watcher->stoped = true;
    watcher->root_dirs = r_dirs;
    watcher->should_include_dir = should_include_dir;
    watcher->should_include_file_change = should_include_file_change;
    watcher->context = context;

    watcher->watched_dirs =
        hashmap_new(sizeof(struct watched_dir), 0, 0, 0, str_hash, str_compare, watched_dir_free, NULL);

    watcher->event_batch.dir_structure_changed = false;
    watcher->event_batch.file_events = vec_create(struct watcher_file_event);

    pthread_mutex_t *lock = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(lock, NULL) != 0)
    {
        free(lock);
        log_critical("(watcher) Unable to initialize watcher lock");
        return NULL;
    }

    pthread_cond_t *cond = malloc(sizeof(pthread_cond_t));
    if (pthread_cond_init(cond, NULL) != 0)
    {
        free(lock);
        free(cond);
        log_critical("(watcher) Unable to initialize watcher cond\n");
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

    watcher_free_event_batch(watcher->event_batch);

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

    return 0;
}

int
watcher_start(struct watcher *watcher)
{
    if (watcher->thr)
    {
        log_error("(watcher) Unable to start watcher because it has already started\n");
        return 1;
    }

    log_info("(watcher) Starting\n");

    pthread_t thr;
    watcher->stoped = false;
    pthread_create(&thr, NULL, watcher_start_watching_thr, (void *)watcher);
    watcher->thr = thr;

    return 0;
}

int
watcher_signal_stop(struct watcher *watcher)
{
    log_info("(watcher) Signal stopping\n");

    watcher->stoped = true;
    pthread_cond_broadcast(watcher->cond);

    return 0;
}

int
watcher_wait_for_stop(struct watcher *watcher)
{
    if (watcher->thr <= 0)
        return 1;

    pthread_cond_broadcast(watcher->cond);
    pthread_join(watcher->thr, NULL);

    watcher->thr = -1;

    return 0;
}

int
watcher_read_event_batch(struct watcher *watcher, int debounce_ms, struct watcher_event_batch *batch)
{
    pthread_mutex_lock(watcher->lock);
    for (; !watcher->stoped;)
    {
        bool batch_is_empty = !watcher->event_batch.dir_structure_changed &&
                              !vec_length(watcher->event_batch.file_events);

        time_t current_ms = get_current_timestamp_in_ms();
        bool should_debounce = watcher->event_batch.latest_change_timestamp && debounce_ms &&
                               ((watcher->event_batch.latest_change_timestamp + debounce_ms) > current_ms);

        if (batch_is_empty)
        {
            pthread_cond_wait(watcher->cond, watcher->lock);
            pthread_mutex_unlock(watcher->lock);
            continue;
        }
        else if (should_debounce)
        {
            time_t debounce_diff = current_ms - watcher->event_batch.latest_change_timestamp + debounce_ms;
            pthread_mutex_unlock(watcher->lock);
            // there is no need to even cond_wait because each subsequent change will only increase timestamp
            sleep_ms(debounce_diff);
            continue;
        }

        memcpy(batch, &watcher->event_batch, sizeof(struct watcher_event_batch));
        watcher->event_batch.dir_structure_changed = false;
        watcher->event_batch.file_events = vec_create(struct watcher_file_event);
        watcher->event_batch.latest_change_timestamp = 0;
        pthread_mutex_unlock(watcher->lock);
        return 0;
    }

    return -1;
}

int
watcher_free_event_batch(struct watcher_event_batch batch)
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
    batch.latest_change_timestamp = 0;

    return 0;
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
        if (select_status <= 0 || !FD_ISSET(notify_fd, &read_fds) || watcher->stoped)
            continue;

        int length = read(notify_fd, buffer, BUF_LEN);

        bool dir_action = false;
        bool file_action = false;
        for (int i = 0; i < length;)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            i += EVENT_SIZE + event->len;

            dir_action = event->mask & DIR_EVENT_MASK;
            if (event->mask & (DIR_EVENT_MASK | IN_IGNORED))
                continue;

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

            if (watcher->should_include_file_change &&
                !watcher->should_include_file_change(event_dir, event->name, watcher->context))
            {
                log_debug("(watcher) File '%s' in directory '%s' change ignored\n", event->name, event_dir);
                continue;
            }

            file_action = true;

            bool created = event->mask & (IN_CREATE | IN_MOVED_TO);
            bool deleted = event->mask & (IN_DELETE | IN_MOVED_FROM);
            bool modified = event->mask & IN_CLOSE_WRITE;
            bool timestamp = get_current_timestamp_in_ms();

            pthread_mutex_lock(watcher->lock);

            bool found = false;
            vec_for_each2(struct watcher_file_event, fe, watcher->event_batch.file_events)
            {
                if (strcmp(fe->file_name, event->name) == 0 && strcmp(fe->dir, event_dir) == 0)
                {
                    fe->created |= created;
                    fe->deleted |= deleted;
                    fe->modified |= modified;
                    fe->timestamp = timestamp;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                struct watcher_file_event file_event = { .dir = str_dup(event_dir),
                                                         .file_name = str_dup(event->name),
                                                         .created = created,
                                                         .deleted = deleted,
                                                         .modified = modified,
                                                         .timestamp = timestamp };
                vec_push(watcher->event_batch.file_events, file_event);
            }

            pthread_mutex_unlock(watcher->lock);
        }

        if (dir_action)
            init_and_add_watched_dirs(watcher, notify_fd);

        pthread_mutex_lock(watcher->lock);

        watcher->event_batch.dir_structure_changed |= dir_action;

        if (dir_action || file_action)
            watcher->event_batch.latest_change_timestamp = get_current_timestamp_in_ms();

        pthread_mutex_unlock(watcher->lock);

        if (dir_action || file_action)
            pthread_cond_broadcast(watcher->cond);
    }

    hashmap_clear(watcher->watched_dirs, 0);
    close(notify_fd);

    return 0;
}

static void
init_and_add_watched_dirs(struct watcher *watcher, int notify_fd)
{
    struct hashmap *scanned_dirs_map =
        hashmap_new(sizeof(char *), 0, 0, 0, str_hash, str_compare, pointer_to_string_free, NULL);
    vec_for_each2(char *, r_dir, watcher->root_dirs)
    {
        vec_scoped char **dirs = get_directories_recursive(*r_dir, watcher->should_include_dir, watcher->context);
        vec_for_each2(char *, dir_p, dirs)
        {
            char *dir = *dir_p;
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
        if (hashmap_get(scanned_dirs_map, &w_dir->dir))
            continue;

        // TODO: this is not ideal, find other way to iterate it without having to reset the cursor after each delete
        iter_wd = 0;
        // We need to copy here because after hashmap_delete pointer moves
        struct watched_dir w_dir_cp = *w_dir;
        hashmap_delete(watcher->watched_dirs, &w_dir_cp);

        inotify_rm_watch(w_dir->notify_fd, w_dir_cp.wd);
        watched_dir_free((void *)&w_dir_cp);
    }

    size_t iter_sd = 0;
    void *item_sd;
    while (hashmap_iter(scanned_dirs_map, &iter_sd, &item_sd))
    {
        char **dir = item_sd;
        struct watched_dir search_dir = { .dir = *dir };

        if (hashmap_get(watcher->watched_dirs, &search_dir))
            continue;

        struct watched_dir wtd;
        wtd.dir = str_dup(*dir);
        wtd.wd = inotify_add_watch(notify_fd, *dir, WATCHED_EVENT_MASK);
        wtd.notify_fd = notify_fd;

        hashmap_set(watcher->watched_dirs, &wtd);
    }

    hashmap_free(scanned_dirs_map);

    log_debug("(watcher) Now watching %ld dirs\n", hashmap_count(watcher->watched_dirs));
}

static void
watched_dir_free(void *item)
{
    struct watched_dir *w_dir = item;
    free(w_dir->dir);
}

static void
pointer_to_string_free(void *item)
{
    char **str = item;
    free(*str);
}

static int
str_compare(const void *a, const void *b, void *udata)
{
    (void)udata;
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

static char **
get_directories_recursive(const char *root_dir, bool (*should_include_dir)(char *, void *), void *context)
{
    char **dirs = vec_create_prealloc(char *, 8);

    vec_scoped char **stack = vec_create_prealloc(char *, 8);
    vec_push(stack, str_dup(root_dir));

    while (vec_length(stack))
    {
        char *current_dir = NULL;
        vec_pop(stack, &current_dir);

        DIR *dir = opendir(current_dir);
        if (dir == NULL || (should_include_dir && !should_include_dir(current_dir, context)))
        {
            if (dir != NULL)
                log_debug("(watcher) Directory '%s' ignored\n", current_dir);

            closedir(dir);
            free(current_dir);
            continue;
        }

        vec_push(dirs, current_dir);

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strncmp(entry->d_name, ".", 1) == 0 || strncmp(entry->d_name, "..", 2) == 0)
                continue;

            const int dt_dir = 4;
            if (entry->d_type != dt_dir)
                continue;
            char *dir_path = paths_join(current_dir, entry->d_name);

            vec_push(stack, dir_path);
        }

        closedir(dir);
    };

    return dirs;
}
