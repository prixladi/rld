#ifndef WATCHER__H
#define WATCHER__H

struct watcher;

struct watcher_file_event
{
    char *file_name;
    char *dir;
    time_t timestamp;

    bool created;
    bool deleted;
    bool modified;
};

struct watcher_event_batch
{
    struct watcher_file_event *file_events;
    time_t latest_change_timestamp;
    bool dir_structure_changed;
};

struct watcher *watcher_create(char **root_dirs, bool (*should_include_dir)(char *),
                               bool (*should_include_file_change)(char *, char *));
int watcher_start(struct watcher *watcher);

int watcher_signal_stop(struct watcher *watcher);
int watcher_wait_stop(struct watcher *watcher);
int watcher_free(struct watcher *watcher);

int watcher_read_event_batch(struct watcher *watcher, int debounce_ms, struct watcher_event_batch *batch);
int watcher_clear_event_batch(struct watcher_event_batch *batch);

#endif
