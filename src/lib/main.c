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

#include "main.h"
#include "watcher.h"
#include "runner.h"

struct watcher_state
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

struct executor_state
{
    char **build_command;
    char **run_command;

    pthread_t thr;
    bool exiting;

    pid_t build_pid;
    pid_t run_pid;

    pthread_mutex_t *lock;
};

static void *
watcher_loop(void *data)
{
    struct watcher_state *state = data;

    int res = 0;
    struct watcher_event_batch batch;
    while ((res = watcher_read_event_batch(state->watcher, &batch)) == 0)
    {
        pthread_mutex_lock(state->lock);

        vec_for_each2(struct watcher_file_event, event, batch.file_events)
        {
            bool found = false;
            vec_for_each2(struct changed_file, changed_file, state->changed_files)
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

                vec_push(state->changed_files, changed_file);
            }
        }

        state->dir_structure_changed |= batch.dir_structure_changed;

        state->last_change_timestamp = get_current_timestamp_in_ms();

        pthread_mutex_unlock(state->lock);
        watcher_clear_event_batch(batch);

        pthread_cond_broadcast(state->cond);
    }
}

static void *
executor(void *data)
{
    struct executor_state *state = data;

    pthread_mutex_lock(state->lock);

    if (state->exiting)
    {
        pthread_mutex_unlock(state->lock);
        return NULL;
    }
    log_info("Starting build command\n");
    pid_t pid = process_start(state->build_command);
    state->build_pid = pid;

    pthread_mutex_unlock(state->lock);

    int status_code = process_wait(pid);

    pthread_mutex_lock(state->lock);
    state->build_pid = 0;

    if (!state->exiting)
    {
        pthread_mutex_unlock(state->lock);

        if (status_code == 0)
        {
            log_info("Build command completed successfully\n");

            pthread_mutex_lock(state->lock);

            log_info("Starting run command\n");
            pid_t run_pid = process_start(state->run_command);

            state->run_pid = run_pid;

            pthread_mutex_unlock(state->lock);

            int run_status_code = process_wait(run_pid);

            pthread_mutex_lock(state->lock);

            state->run_pid = 0;

            if (state->run_pid == state->run_pid)
            {
                if (status_code == 0)
                    log_info("Run command completed successfully\n");
                else
                    log_error("Run exited with error status code %d\n", status_code);
            }
            else
                log_info("Run command interupted\n");

            pthread_mutex_unlock(state->lock);
        }
        else
            log_error("Build exited with error status code %d\n", status_code);
    }
    else
    {
        pthread_mutex_unlock(state->lock);
        log_info("Build command interupted\n");
    }

    return NULL;
}

struct watcher_state *watcher_state_g = NULL;
bool exiting_g = false;

static void
graceful_stop_handler(int signal)
{
    (void)signal;
    // Intentionally not using 'log_*' or 'printf' because it uses non-async-signal-safe functions
    write(STDOUT_FILENO, "[SGN] Received terminate signal, stopping\n", 43);

    exiting_g = true;
    if (watcher_state_g)
    {
        watcher_signal_stop(watcher_state_g->watcher);
        pthread_cond_broadcast(watcher_state_g->cond);
    }
}

static void
watcher_state_free(struct watcher_state *state)
{
    watcher_free(state->watcher);
    vec_free(state->changed_files);

    pthread_cond_destroy(state->cond);
    free(state->cond);

    pthread_mutex_destroy(state->lock);
    free(state->lock);

    state->watcher = NULL;
    state->changed_files = NULL;

    state->lock = NULL;
    state->cond = NULL;
}

static void
executor_state_free(struct executor_state *state)
{
    pthread_mutex_destroy(state->lock);
    free(state->lock);

    state->lock = NULL;
}

static void
changed_file_free(struct changed_file cf)
{
    free(cf.file_name);
    free(cf.dir);

    cf.file_name = NULL;
    cf.dir = NULL;
}

static void
changes_context_free(struct changes_context cf)
{
    vec_for_each(cf.changed_files, changed_file_free);
    vec_free(cf.changed_files);
    cf.changed_files = NULL;
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
    struct config config = create_config(&context);

    if (config.work_dir)
        chdir(config.work_dir);

    struct watcher *watcher = watcher_create(config.watch_paths, &should_include_dir);

    watcher_start_watching(watcher);

    pthread_mutex_t *lock = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(lock, NULL) != 0)
    {
        log_critical("Unable to initialize state lock");
        return 1;
    }

    pthread_cond_t *cond = malloc(sizeof(pthread_cond_t));
    if (pthread_cond_init(cond, NULL) != 0)
    {
        log_critical("Unable to initialize state cond\n");
        return 1;
    }

    pthread_mutex_t *executor_lock = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(executor_lock, NULL) != 0)
    {
        log_critical("Unable to initialize state build lock");
        return 1;
    }

    struct executor_state executor_state = { .lock = executor_lock,
                                             .build_command = NULL,
                                             .run_command = NULL,
                                             .exiting = false,
                                             .thr = 0,
                                             .build_pid = 0,
                                             .run_pid = 0 };

    struct watcher_state watcher_state = {
        .watcher = watcher,
        .changed_files = vec_create(struct changed_file),
        .lock = lock,
        .cond = cond,
        .thr = 0,
        .exiting = false,
    };

    watcher_state_g = &watcher_state;

    pthread_t read_loop_thr;
    pthread_create(&read_loop_thr, NULL, watcher_loop, (void *)&watcher_state);

    bool is_first = true;
    for (; !exiting_g;)
    {
        bool is_first_run = is_first;
        is_first = false;

        pthread_mutex_lock(watcher_state.lock);

        bool no_change = !watcher_state.dir_structure_changed && vec_length(watcher_state.changed_files) == 0;

        time_t current_ms = get_current_timestamp_in_ms();

        bool should_debounce = watcher_state.last_change_timestamp && config.debounce_ms &&
                               ((watcher_state.last_change_timestamp + config.debounce_ms) > current_ms);

        if (!is_first_run && (no_change || should_debounce))
        {
            if (no_change)
            {
                pthread_cond_wait(watcher_state.cond, watcher_state.lock);
            }
            else
            {
                time_t debounce_diff = current_ms - watcher_state.last_change_timestamp + config.debounce_ms;

                struct timeval tp;
                struct timespec ts;
                int rc = gettimeofday(&tp, NULL);

                ts.tv_sec = tp.tv_sec;
                ts.tv_nsec = tp.tv_usec * 1000;

                ts.tv_nsec += debounce_diff * 1000000;

                ts.tv_sec += ts.tv_nsec / 1000000000L;
                ts.tv_nsec = ts.tv_nsec % 1000000000L;

                pthread_cond_timedwait(watcher_state.cond, watcher_state.lock, &ts);
            }

            pthread_mutex_unlock(watcher_state.lock);
            continue;
        }

        if (executor_state.thr)
        {
            executor_state.exiting = true;

            if (executor_state.run_pid)
                process_kill(executor_state.run_pid);
            if (executor_state.build_pid)
                process_kill(executor_state.build_pid);

            pthread_mutex_unlock(watcher_state.lock);

            pthread_join(executor_state.thr, NULL);
            executor_state.exiting = false;
            executor_state.thr = 0;

            if (executor_state.build_command)
            {
                free_build_command(executor_state.build_command, &context);
                executor_state.build_command = NULL;
            }
            if (executor_state.run_command)
            {
                free_build_command(executor_state.run_command, &context);
                executor_state.run_command = NULL;
            }

            continue;
        }

        struct changes_context changes_context = { .changed_files = vec_create(struct changed_file *),
                                                   .dir_structure_changed = watcher_state.dir_structure_changed,
                                                   .is_first_run = is_first_run };

        vec_for_each2(struct changed_file, cf, watcher_state.changed_files)
        {
            if (should_include_file_change(cf))
                vec_push(changes_context.changed_files, *cf);
            else
                (changed_file_free(*cf));
        }

        bool should_rebuild = changes_context.dir_structure_changed || vec_length(changes_context.changed_files) ||
                              changes_context.is_first_run;

        watcher_state.dir_structure_changed = false;
        _vector_field_set(watcher_state.changed_files, LENGTH, 0);

        pthread_mutex_unlock(watcher_state.lock);

        if (!should_rebuild)
        {
            changes_context_free(changes_context);
            continue;
        }

        executor_state.build_command = get_build_command(&changes_context, &context);
        executor_state.run_command = get_run_command(&changes_context, &context);

        changes_context_free(changes_context);

        pthread_t executor_thr;
        pthread_create(&executor_thr, NULL, executor, (void *)&executor_state);
        executor_state.thr = executor_thr;
    }

    if (executor_state.thr)
    {
        executor_state.exiting = true;

        if (executor_state.run_pid)
            process_kill(executor_state.run_pid);
        if (executor_state.build_pid)
            process_kill(executor_state.build_pid);

        pthread_mutex_unlock(watcher_state.lock);

        pthread_join(executor_state.thr, NULL);
        executor_state.exiting = false;
        executor_state.thr = 0;

        if (executor_state.build_command)
        {
            free_build_command(executor_state.build_command, &context);
            executor_state.build_command = NULL;
        }
        if (executor_state.run_command)
        {
            free_build_command(executor_state.run_command, &context);
            executor_state.run_command = NULL;
        }
    }

    watcher_join(watcher);
    pthread_join(read_loop_thr, NULL);

    watcher_state_free(&watcher_state);
    executor_state_free(&executor_state);

    free_config(&config);
    return 0;
}
