#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/time.h>

#include "utils/vector.h"
#include "utils/string.h"
#include "utils/log.h"
#include "utils/time.h"
#include "utils/memory.h"

#include "main.h"
#include "watcher.h"
#include "executor.h"

struct reader
{
    struct watcher *watcher;

    pthread_t thr;
    bool exiting;

    struct changed_file *changed_files;
    bool dir_structure_changed;
    time_t last_change_timestamp;

    pthread_mutex_t *lock;
    pthread_cond_t *cond;
};

static void
changed_file_free(struct changed_file cf)
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
            vec_for_each2(struct changed_file, changed_file, reader->changed_files)
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
                struct changed_file changed_file = { .created = event->created || event->moved_to,
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

static int
reader_init(struct reader *reader)
{
    pthread_mutex_t *lock = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(lock, NULL) != 0)
    {
        log_critical("Unable to initialize reader lock");
        return 1;
    }

    pthread_cond_t *cond = malloc(sizeof(pthread_cond_t));
    if (pthread_cond_init(cond, NULL) != 0)
    {
        log_critical("Unable to initialize reader cond\n");
        return 1;
    }

    reader->changed_files = vec_create(struct changed_file);
    reader->lock = lock;
    reader->cond = cond;
    reader->thr = 0;
    reader->exiting = false;

    return 0;
}

static int
reader_start(struct reader *reader, char **watch_paths, bool (*should_include_dir)(char *),
             bool (*should_include_file_change)(char *, char *))
{
    if (reader->thr)
    {
        log_critical("Unable to start reader because it is already started.\n");
        return 1;
    }

    struct watcher *watcher = watcher_create(watch_paths, should_include_dir, should_include_file_change);
    if (!watcher)
    {
        log_critical("Unable to initialize watcher.\n");
        return 2;
    }
    watcher_start_watching(watcher);

    reader->watcher = watcher;

    pthread_t thr;
    pthread_create(&thr, NULL, reader_loop, (void *)reader);
    reader->thr = thr;

    return 0;
}

static void
reader_free(struct reader *reader)
{
    watcher_free(reader->watcher);
    vec_for_each(reader->changed_files, changed_file_free);
    vec_free(reader->changed_files);

    pthread_cond_destroy(reader->cond);
    free(reader->cond);

    pthread_mutex_destroy(reader->lock);
    free(reader->lock);

    reader->watcher = NULL;
    reader->changed_files = NULL;

    reader->lock = NULL;
    reader->cond = NULL;
}

static int
reader_signal_stop(struct reader *reader)
{
    watcher_signal_stop(reader->watcher);
    pthread_cond_broadcast(reader->cond);
    return 0;
}

static int
reader_join(struct reader *reader)
{
    if (!reader->thr)
        return 1;
    pthread_join(reader->thr, NULL);
    return 0;
}

static void
changes_context_free(struct changes_context cf)
{
    vec_for_each(cf.changed_files, changed_file_free);
    vec_free(cf.changed_files);
    cf.changed_files = NULL;
}

struct reader *reader_g = NULL;
bool exiting_g = false;

static void
graceful_stop_handler(int signal)
{
    (void)signal;
    // Intentionally not using 'log_*' or 'printf' because it uses non-async-signal-safe functions
    write(STDOUT_FILENO, "[SGN] Received terminate signal, stopping\n", 43);

    exiting_g = true;
    if (reader_g)
        reader_signal_stop(reader_g);
}

int
entrypoint(int argc, char **argv)
{
    log_init(INFO);

    signal(SIGPIPE, SIG_IGN);

    signal(SIGTERM, graceful_stop_handler);
    signal(SIGINT, graceful_stop_handler);
    signal(SIGHUP, graceful_stop_handler);

    struct context context = {};
    struct config config = config_create(&context);

    if (config.work_dir)
        chdir(config.work_dir);

    struct reader reader = { 0 };
    if (reader_init(&reader) != 0)
    {
        log_critical("Unable to initialize reader.\n");
        return 1;
    }
    if (reader_start(&reader, config.watch_paths, &should_include_dir, &should_include_file_change) != 0)
    {
        log_critical("Unable to start reader.\n");
        return 0;
    }
    reader_g = &reader;

    struct executor *executor = executor_create();
    if (!executor)
    {
        log_critical("Unable to initialize watcher.\n");
        return 1;
    }

    for (bool is_first_run = true; !exiting_g; is_first_run = false)
    {
        pthread_mutex_lock(reader.lock);

        bool no_change = !reader.dir_structure_changed && vec_length(reader.changed_files) == 0;
        time_t current_ms = get_current_timestamp_in_ms();
        bool should_debounce = reader.last_change_timestamp && config.debounce_ms &&
                               ((reader.last_change_timestamp + config.debounce_ms) > current_ms);

        if (!is_first_run)
        {
            if (no_change)
            {
                pthread_cond_wait(reader.cond, reader.lock);
                pthread_mutex_unlock(reader.lock);
                continue;
            }
            else if (should_debounce)
            {
                time_t debounce_diff = current_ms - reader.last_change_timestamp + config.debounce_ms;
                pthread_mutex_unlock(reader.lock);
                // there is no need to even cond_wait because each subsequent change will only increase timestamp
                sleep_ms(debounce_diff);
                continue;
            }
        }

        bool should_rebuild = reader.dir_structure_changed || vec_length(reader.changed_files) || is_first_run;

        reader.dir_structure_changed = false;
        _vector_field_set(reader.changed_files, LENGTH, 0);

        pthread_mutex_unlock(reader.lock);

        if (!should_rebuild)
            continue;

        struct changes_context changes_context = { .changed_files = vec_dup(reader.changed_files),
                                                   .dir_structure_changed = reader.dir_structure_changed,
                                                   .is_first_run = is_first_run };

        executor_stop_join_clear(executor);

        struct command *commands = commands_create(&changes_context, &context);
        changes_context_free(changes_context);

        struct executor_command *executor_commands = vec_create_prealloc(struct executor_command, vec_length(commands));
        vec_for_each2(struct command, command, commands)
        {
            // + 1 is for NULL terminator
            char **exec = vec_create_prealloc(char *, vec_length(command->exec) + 1);
            vec_for_each2(char *, e, command->exec)
            {
                char *c = str_dup(*e);
                vec_push(exec, c);
            }
            vec_push(exec, NULL);

            struct executor_command ec = { .name = str_dup(command->name), .exec = exec, .pid = 0 };
            vec_push(executor_commands, ec);
        }
        commands_free(commands, &context);

        executor_start(executor, executor_commands);
    }

    reader_signal_stop(&reader);
    reader_join(&reader);
    reader_free(&reader);

    executor_stop_join_clear(executor);
    executor_free(executor);

    config_free(&config);
    return 0;
}
