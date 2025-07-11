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
    bool moved_from;
    bool moved_to;
};

struct watcher_event_batch
{
    bool dir_structure_changed;
    struct watcher_file_event *file_events;
};

struct watcher *watcher_create(char **root_dirs, bool (*should_include_dir)(char *),
                               bool (*should_include_file_change)(char *, char *));
int watcher_free(struct watcher *watcher);

int watcher_start(struct watcher *watcher);
int watcher_signal_stop(struct watcher *watcher);
int watcher_join(struct watcher *watcher);

int watcher_read_event_batch(struct watcher *watcher, struct watcher_event_batch *batch);
int watcher_clear_event_batch(struct watcher_event_batch *batch);

#endif
