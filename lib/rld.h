#ifndef INTERFACE__H
#define INTERFACE__H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

// Expands to the main function that serves as the entry point of the application
// It just calls `int rld(int argc, char **argv)` and returns its result
// If you need to do some logic before the `rld` is called, you can implement your own main directly
#define __RLD_MAIN \
    int main(int argc, char **argv) \
    { \
        rld(argc, argv); \
    }

// Rld entrypoint, takes over program execution
int rld(int argc, char **argv);

struct key_value
{
    char *key;
    char *value;
};

// Wrapper around parsed command line arguments
struct args
{
    // Short flags, eg. -f, -vvvol
    char *flags;
    // Long  flags, eg. --help, --verbose --verbose
    char **long_flags;
    // Loose values, eg. service, x
    char **values;
    // Key value pairs, eg. service=api, a=b
    struct key_value *key_values;
};

// Application config
// After the call of `init_config`, this structure will be passed in the context argument to all subsequent calls of interface functions
struct config
{
    // An array of paths that should be watched 
    // Needs to be initialized with `vec_create`
    char **watch_paths;
    // Optional work directory
    char *work_dir;
    // Debounce in milliseconds that will delay command execution after a file change
    int debounce_ms;

    // Generic user data
    void *user_data;
};

// Application context
struct context
{
    char *version;
    // User-defined application config
    struct config config;
    // Parsed command line arguments
    struct args args;
};

struct changed_file
{
    char *file_name;
    char *dir;

    bool created;
    bool deleted;
    bool modified;
};

// Batch of changed files
struct changes_context
{
    // Array of changes files, it is vector initialized with `vec_create`
    // Vector methods can be used on it such as `vec_length`
    struct changed_file *changed_files;
    // Flag determining whether file structure changed in current changes batch
    bool dir_structure_changed;
    // Flag determining whether it is first application loop run
    bool is_first_run;
};

// Env of the command
struct command_env
{
    char *key;
    char *value;
    // Flag determining whether this ENV should not override ENV existing in current execution
    bool no_override;
};

// Command to be executed by librld
struct command
{
    // Name of the command for output purposes
    char *name;
    // Tokenized command, it needs to be an array of strings created with `vec_create`
    // Should not contain trailing NULL
    char **exec;
    // Optional workdir
    char *work_dir;
    // Optional array of env for commands, needs to be initialized with `vec_create`
    struct command_env *env;
    // Flag determining whether the application should not be interrupted with a batch of changes and instead left to exit on its own
    bool no_interrupt;
};

// INTERFACE

// Initializes a application config
// Returns 0 if the initialization was successful
int config_init(struct config *config, struct context *context);

// Cleanup for initialized config
// If `config_init` did not perform any action requiring cleanup, this function can be left empty
void config_free(struct config *config, struct context *context);

// Creates array of commands to be executed.
// It is called with the context of changes in batch, so it can implement specific logic depending on the changed files or directories
// The array itself needs to be created using the `vec_create` function
struct command *commands_create(struct changes_context *changes_context, struct context *context);

// Cleanup function for array of commands
// The array itself needs to be freed using `vec_free` function
void commands_free(struct command *commands, struct context *context);

// Determines whether the application should watch concrete directory
// It is called every time new directory if found in watched path
bool should_include_dir(char *dir, struct context *context);

// Determines whether the application should track file concrete file change
// It is called every time file in tracked directory is create, modified or deleted
bool should_include_file_change(char *dir, char *file_name, struct context *context);

#endif
