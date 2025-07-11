#include <pthread.h>
#include <stdbool.h>

#include "utils/vector.h"
#include "utils/log.h"
#include "utils/time.h"

#include "reader.h"
#include "watcher.h"

struct reader
{
    struct watcher *watcher;

    pthread_t thr;
    bool exiting;

    struct reader_changed_file *changed_files;
    bool dir_structure_changed;
    time_t last_change_timestamp;

    pthread_mutex_t *lock;
    pthread_cond_t *cond;
};

static void reader_changed_file_free(struct reader_changed_file cf);
static void *reader_loop(void *data);

struct reader *
reader_create()
{
    struct reader *reader = calloc(1, sizeof(struct reader));

    pthread_mutex_t *lock = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(lock, NULL) != 0)
    {
        log_critical("(reader) Unable to initialize lock");
        return NULL;
    }

    pthread_cond_t *cond = malloc(sizeof(pthread_cond_t));
    if (pthread_cond_init(cond, NULL) != 0)
    {
        log_critical("(reader) Unable to initialize cond\n");
        return NULL;
    }

    reader->changed_files = vec_create(struct reader_changed_file);
    reader->lock = lock;
    reader->cond = cond;
    reader->thr = 0;
    reader->exiting = false;

    return reader;
}

int
reader_start(struct reader *reader, struct watcher *watcher, bool (*should_include_dir)(char *),
             bool (*should_include_file_change)(char *, char *))
{
    if (reader->thr)
    {
        log_critical("(reader) Unable to start reader because it is already started.\n");
        return 1;
    }

    reader->watcher = watcher;

    log_info("(reader) Starting\n");
    pthread_t thr;
    pthread_create(&thr, NULL, reader_loop, (void *)reader);
    reader->thr = thr;

    return 0;
}

struct reader_changes_context *
reader_wait_for_data_with_debounce(struct reader *reader, int debounce_ms)
{
    for (; !reader->exiting;)
    {
        pthread_mutex_lock(reader->lock);

        bool no_change = !reader->dir_structure_changed && vec_length(reader->changed_files) == 0;
        time_t current_ms = get_current_timestamp_in_ms();
        bool should_debounce = reader->last_change_timestamp && debounce_ms &&
                               ((reader->last_change_timestamp + debounce_ms) > current_ms);

        if (no_change)
        {
            pthread_cond_wait(reader->cond, reader->lock);
            pthread_mutex_unlock(reader->lock);
            continue;
        }
        else if (should_debounce)
        {
            time_t debounce_diff = current_ms - reader->last_change_timestamp + debounce_ms;
            pthread_mutex_unlock(reader->lock);
            // there is no need to even cond_wait because each subsequent change will only increase timestamp
            sleep_ms(debounce_diff);
            continue;
        }

        struct reader_changes_context *changes_context = calloc(1, sizeof(struct reader_changes_context));

        changes_context->changed_files = vec_dup(reader->changed_files);
        changes_context->dir_structure_changed = reader->dir_structure_changed;

        reader->dir_structure_changed = false;
        _vector_field_set(reader->changed_files, LENGTH, 0);

        pthread_mutex_unlock(reader->lock);

        return changes_context;
    }

    return NULL;
}

int
reader_signal_stop(struct reader *reader)
{
    pthread_mutex_lock(reader->lock);
    log_info("(reader) Stoping\n");

    reader->exiting = true;
    watcher_signal_stop(reader->watcher);
    pthread_cond_broadcast(reader->cond);

    pthread_mutex_unlock(reader->lock);

    return 0;
}

int
reader_join(struct reader *reader)
{
    if (!reader->thr)
        return 1;
    pthread_join(reader->thr, NULL);
    return 0;
}

void
reader_free(struct reader *reader)
{
    watcher_free(reader->watcher);
    vec_for_each(reader->changed_files, reader_changed_file_free);
    vec_free(reader->changed_files);

    pthread_cond_destroy(reader->cond);
    free(reader->cond);

    pthread_mutex_destroy(reader->lock);
    free(reader->lock);

    reader->watcher = NULL;
    reader->changed_files = NULL;

    reader->lock = NULL;
    reader->cond = NULL;
    free(reader);
}

void
reader_changes_context_free(struct reader_changes_context *cf)
{
    vec_for_each(cf->changed_files, reader_changed_file_free);
    vec_free(cf->changed_files);
    cf->changed_files = NULL;
    free(cf);
}

static void
reader_changed_file_free(struct reader_changed_file cf)
{
    free(cf.file_name);
    free(cf.dir);

    cf.file_name = NULL;
    cf.dir = NULL;
}

static void *
reader_loop(void *data)
{
    struct reader *reader = data;

    int res = 0;
    struct watcher_event_batch batch;
    while ((res = watcher_read_event_batch(reader->watcher, &batch)) == 0)
    {
        pthread_mutex_lock(reader->lock);

        vec_for_each2(struct watcher_file_event, event, batch.file_events)
        {
            bool found = false;
            vec_for_each2(struct reader_changed_file, changed_file, reader->changed_files)
            {
                if (strcmp(event->file_name, changed_file->file_name) == 0 && strcmp(event->dir, changed_file->dir) == 0)
                {
                    changed_file->created |= event->created || event->moved_to;
                    changed_file->deleted |= event->deleted || event->moved_from;
                    changed_file->modified |= event->modified;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                struct reader_changed_file changed_file = { .created = event->created || event->moved_to,
                                                            .deleted = event->deleted || event->moved_from,
                                                            .modified = event->modified,
                                                            .dir = str_dup(event->dir),
                                                            .file_name = str_dup(event->file_name) };

                vec_push(reader->changed_files, changed_file);
            }
        }

        reader->dir_structure_changed |= batch.dir_structure_changed;

        reader->last_change_timestamp = get_current_timestamp_in_ms();

        pthread_mutex_unlock(reader->lock);
        watcher_clear_event_batch(&batch);

        pthread_cond_broadcast(reader->cond);
    }
}
