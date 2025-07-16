#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>

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

static void graceful_stop_handler(int signal);
static void changed_file_free(struct changed_file cf);
static void changes_context_free(struct changes_context cf);

static struct args args_parse(int argc, char **argv);
static void args_free(struct args args);

struct watcher *watcher_g = NULL;
bool stopping_g = false;

#ifndef VERSION
#define VERSION "0"
#endif

int
entrypoint(int argc, char **argv)
{
    log_init(INFO);

    signal(SIGPIPE, SIG_IGN);

    signal(SIGTERM, graceful_stop_handler);
    signal(SIGINT, graceful_stop_handler);
    signal(SIGHUP, graceful_stop_handler);

    struct context context = { .version = VERSION, .args = args_parse(argc, argv) };
    struct config config = config_create(&context);

    if (config.work_dir)
        chdir(config.work_dir);

    struct watcher *watcher = watcher_create(config.watch_paths, should_include_dir, should_include_file_change);
    if (!watcher)
    {
        log_critical("Unable to initialize watcher.\n");
        return 1;
    }
    watcher_g = watcher;

    struct executor *executor = executor_create();
    if (!executor)
    {
        watcher_free(watcher);
        log_critical("Unable to initialize executor.\n");
        return 2;
    }

    if (watcher_start(watcher) != 0)
    {
        watcher_free(watcher);
        executor_free(executor);
        log_critical("Unable to start watcher.\n");
        return 3;
    }

    for (bool is_first_run = true; !stopping_g; is_first_run = false)
    {
        struct changes_context changes_context = { .changed_files = vec_create(struct changed_file),
                                                   .dir_structure_changed = false,
                                                   .is_first_run = is_first_run };

        if (!is_first_run)
        {
            struct watcher_event_batch batch = { 0 };
            int result = watcher_read_event_batch(watcher, config.debounce_ms, &batch);
            // watcher stopped that means that whole app should stop
            if (result != 0)
            {
                changes_context_free(changes_context);
                break;
            }

            changes_context.dir_structure_changed |= batch.dir_structure_changed;
            vec_for_each2(struct watcher_file_event, f, batch.file_events)
            {
                struct changed_file cf = { .dir = str_dup(f->dir), .file_name = str_dup(f->file_name) };
                vec_push(changes_context.changed_files, cf);
            }
            watcher_free_event_batch(batch);
        }

        executor_stop_commands_and_wait(executor);

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

            struct executor_command ec = {
                .name = str_dup(command->name), .exec = exec, .work_dir = str_dup(command->work_dir), .pid = 0
            };
            vec_push(executor_commands, ec);
        }
        commands_free(commands, &context);

        executor_run_commands(executor, executor_commands);
    }

    if (!stopping_g)
        log_critical("Broken out of the main application loop without global stopping flag set to true");

    watcher_wait_for_stop(watcher);
    watcher_free(watcher);

    executor_stop_commands_and_wait(executor);
    executor_free(executor);

    config_free(config, &context);

    args_free(context.args);
    return 0;
}

static void
graceful_stop_handler(int signal)
{
    (void)signal;
    // Intentionally not using 'log_*' or 'printf' because it uses non-async-signal-safe functions
    write(STDOUT_FILENO, "[SGN] Received terminate signal, stopping\n", 43);

    stopping_g = true;
    if (watcher_g)
        watcher_signal_stop(watcher_g);
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

static struct args
args_parse(int argc, char **argv)
{
    struct args args = {
        .values = vec_create(char *),
        .key_values = vec_create(struct key_value),
        .long_flags = vec_create(char *),
        .flags = vec_create(char),
    };

    for (int i = 1; i < argc; i++)
    {
        char *arg = argv[i];

        if (strlen(arg) > 2 && str_starts_with(arg, "--"))
        {
            vec_push(args.long_flags, str_dup(arg + 2));
            continue;
        }
        if (strlen(arg) > 1 && str_starts_with(arg, "-"))
        {
            for (size_t j = 1; j < strlen(arg); j++)
                vec_push(args.flags, arg[j]);
            continue;
        }

        char *substr_pos = strstr(arg, "=");
        if (substr_pos == NULL)
        {
            vec_push(args.values, str_dup(arg));
            continue;
        }

        ptrdiff_t value_pos = substr_pos - arg;
        struct key_value kv = { .key = str_dup_maxlen(arg, value_pos), .value = str_dup(substr_pos + 1) };
        vec_push(args.key_values, kv);
    }

    const char *args_trace = "ARGS";

    log_trace(args_trace, "Key value args: \n");
    vec_for_each2(struct key_value, kv, args.key_values) log_trace(args_trace, " '%s' = '%s'\n", kv->key, kv->value);

    log_trace(args_trace, "Value args: \n");
    vec_for_each2(char *, v, args.values) log_trace(args_trace, " '%s'\n", *v);

    log_trace(args_trace, "Flags: \n");
    vec_for_each2(char, f, args.flags) log_trace(args_trace, " '%c'\n", *f);

    log_trace(args_trace, "Long flags: \n");
    vec_for_each2(char *, lf, args.long_flags) log_trace(args_trace, " '%s'\n", *lf);

    return args;
}

static void
args_free(struct args args)
{
    vec_for_each(args.values, free);
    vec_for_each(args.long_flags, free);
    vec_for_each2(struct key_value, kv, args.key_values)
    {
        free(kv->key);
        free(kv->value);
    }

    vec_free(args.flags);
    vec_free(args.long_flags);
    vec_free(args.values);
    vec_free(args.key_values);

    args.flags = NULL;
    args.long_flags = NULL;
    args.values = NULL;
    args.key_values = NULL;
}
