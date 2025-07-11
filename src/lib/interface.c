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

#include "interface.h"
#include "watcher.h"
#include "executor.h"
#include "reader.h"

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
    log_init(DEBUG);

    signal(SIGPIPE, SIG_IGN);

    signal(SIGTERM, graceful_stop_handler);
    signal(SIGINT, graceful_stop_handler);
    signal(SIGHUP, graceful_stop_handler);

    struct context context = {};
    struct config config = config_create(&context);

    if (config.work_dir)
        chdir(config.work_dir);

    struct watcher *watcher = watcher_create(config.watch_paths, should_include_dir, should_include_file_change);
    if (!watcher)
    {
        log_critical("Unable to initialize watcher.\n");
        return 1;
    }
    if (watcher_start(watcher) != 0)
    {
        log_critical("Unable to start watcher.\n");
        return 2;
    }

    struct reader *reader = reader_create();
    if (!reader)
    {
        log_critical("Unable to create reader.\n");
        return 3;
    }
    if (reader_start(reader, watcher, &should_include_dir, &should_include_file_change) != 0)
    {
        log_critical("Unable to start reader.\n");
        return 4;
    }
    reader_g = reader;

    struct executor *executor = executor_create();
    if (!executor)
    {
        log_critical("Unable to initialize executor.\n");
        return 5;
    }

    for (bool is_first_run = true; !exiting_g; is_first_run = false)
    {
        struct changes_context changes_context = { .changed_files = vec_create(struct changed_file),
                                                   .dir_structure_changed = false,
                                                   .is_first_run = is_first_run };

        if (!is_first_run)
        {
            struct reader_changes_context *data = reader_wait_for_data_with_debounce(reader, config.debounce_ms);
            // Reader stopped that means that whole app should stop
            if (!data)
            {
                changes_context_free(changes_context);
                break;
            }

            changes_context.dir_structure_changed |= data->dir_structure_changed;
            vec_for_each2(struct reader_changed_file, f, data->changed_files)
            {
                struct changed_file cf = { .dir = str_dup(f->dir), .file_name = str_dup(f->file_name) };
                vec_push(changes_context.changed_files, cf);
            }
            reader_changes_context_free(data);
        }

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

    if (!exiting_g)
        log_critical("Broken out of the main application loop without global exiting flag set to true");

    reader_join(reader);
    reader_free(reader);

    executor_stop_join_clear(executor);
    executor_free(executor);

    config_free(&config);
    return 0;
}
