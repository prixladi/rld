#ifndef READER__H
#define READER__H

#include "watcher.h"

struct reader;

struct reader_changed_file
{
    char *file_name;
    char *dir;

    bool created;
    bool deleted;
    bool modified;
};

struct reader_changes_context
{
    struct reader_changed_file *changed_files;
    bool dir_structure_changed;
};

struct reader *reader_create();
int reader_start(struct reader *reader, struct watcher *watcher, bool (*should_include_dir)(char *),
                 bool (*should_include_file_change)(char *, char *));
struct reader_changes_context *reader_wait_for_data_with_debounce(struct reader *reader, int debounce_ms);
int reader_signal_stop(struct reader *reader);
int reader_join(struct reader *reader);
void reader_free(struct reader *reader);
void reader_changes_context_free(struct reader_changes_context *cf);

#endif
