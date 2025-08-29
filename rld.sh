#! /bin/env bash

set -euo pipefail

NC='\033[0m'

RED="\e[0;31m"
GREEN="\e[0;32m"
BROWN="\e[0;33m"
BLUE="\e[0;34m"
MAGENTA="\e[0;35m"

BOLD_RED="\e[1;31m"
BOLD_GREEN="\e[1;32m"
BOLD_BROWN="\e[1;33m"
BOLD_BLUE="\e[1;34m"
BOLD_MAGENTA="\e[1;35m"

RLD_DIR=".rld"

MAIN=$(cat <<EOF
#include <stdlib.h>

#include <rld/rld.h>
#include <rld/helpers.h>

#include <rld/utils/string.h>
#include <rld/utils/vector.h>
#include <rld/utils/log.h>

// Expands to the main function that serves as the entry point of the application
// It just calls \`int rld(int argc, char **argv)\` and returns its result
// If you need to do some logic before the \`rld\` is called, you can implement your own main directly
__RLD_MAIN

// Initialize a application config
// Returns 0 if the initialization was successful
int config_init(struct config *config, struct context *context)
{
}

// Cleanup for initialized config
// If \`config_init\` did not perform any action requiring cleanup, this function can be left empty
void config_free(struct config *config, struct context *context)
{
}

// Create array of commands to be executed.
// It is called with the context of changes in batch, so it can implement specific logic depending on the changed files or directories
// The array itself needs to be created using the \`vec_create\` function
struct command *commands_create(struct changes_context *changes_context, struct context *context)
{
}

// Cleanup function for array of commands
// The array itself needs to be freed using \`vec_free\` function
void commands_free(struct command *commands, struct context *context)
{
}

// Determine whether the application should watch concrete directory
// It is called every time new directory if found in watched path
bool should_include_dir(char *dir, struct context *context)
{
}

// Determine whether the application should track file concrete file change
// It is called every time file in tracked directory is create, modified or deleted
bool should_include_file_change(char *dir, char *file_name, struct context *context)
{
}

// Print usage
// If you want to print default usage return false otherwise return return true
bool print_usage(const char *app_name)
{
}
EOF
)

GITIGNORE=$(cat <<EOF
.bin
EOF
)

MAKEFILE=$(cat <<EOF
CC=gcc
FLAGS = -W -std=c99 -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -O2

.PHONY: build

setup: 
	mkdir -p .bin

build: setup
	\$(CC) \$(FLAGS) main.c -o ./.bin/rld -lrld

build_debug: FLAGS += -fsanitize=undefined,address -g -D __DEBUG__    
build_debug: setup
	\$(CC) \$(FLAGS) main.c -o ./.bin/rld -lrld
EOF
)

# -------------------------------------------------------------------------------------------------------------------------- #

ensure_rld_exists () {
    if [ ! -d "./$RLD_DIR" ] || [ -z "$( ls -A "./$RLD_DIR" )" ]; then
        echo "ERROR: Rld is not initialized in the current directory, the '$RLD_DIR' directory does exists or it is empty!"
        exit 4;
    fi
}

ensure_rld_does_not_exist () {
    if [ -d "./$RLD_DIR" ] && [ ! -z "$( ls -A "./$RLD_DIR" )" ]; then
        echo "ERROR: Rld is already initialized in the current directory, the '$RLD_DIR' directory exists and it is not empty!"
        exit 9;
    fi
}

run_and_wait () {
    $RLD_DIR/.bin/rld "$@" &
    rld_pid=$!

    trap 'kill -TERM "$rld_pid" 2> /dev/null' EXIT
    wait $rld_pid
}

# -------------------------------------------------------------------------------------------------------------------------- #

rld_help () {
    echo "Usage: $0 <command> [run_arguments]";
    echo "COMMANDS"
    echo "  help"    
    echo "      * print help"
    echo "  init"    
    echo "      * initialize rld in current directory"
    echo "      * this will fail if rld is already initialized in current directory"
    echo "      * initialization will create $RLD_DIR directory"
    echo "  run"     
    echo "     * build and run rld in current directory"
    echo "     * this will fail if rld is not initialized in current directory"
    echo "     * accepts additional arguments that will be passed to rld application"
    echo "     * run '$0 run --help' to get help from rld instance in current directory"
    echo "  debug"     
    echo "     * same as run but builds binary with AddressSanitizer to help you debug crashes and memory leaks"
}

rld_initialize () {
    mkdir -p $RLD_DIR
    echo "$MAIN" > ./$RLD_DIR/main.c
    echo "$GITIGNORE" > ./$RLD_DIR/.gitignore
    echo "$MAKEFILE" > ./$RLD_DIR/Makefile
}

rld_run () {
    cd $RLD_DIR
    make build
    cd ..
    export RLD_EXE="rld run" 
    run_and_wait "${@}"
}

rld_debug () {
    cd $RLD_DIR
    make build_debug
    cd ..
    export RLD_EXE="rld debug" 
    run_and_wait "${@}"
}


# -------------------------------------------------------------------------------------------------------------------------- #


if [ $# -eq 0 ]
  then
    rld_help
    echo "ERROR: No commands supplied"
    exit 126
fi

case $1 in
    
    "init")
        ensure_rld_does_not_exist
        rld_initialize "${@:2}"
    ;;
    
    "run")
        ensure_rld_exists
        rld_run "${@:2}"
    ;;

    "debug")
        ensure_rld_exists
        rld_debug "${@:2}"
    ;;

    "help")
        rld_help "${@}"
    ;;
    
    *)
        rld_help "${@}"
        echo "ERROR: Command $1 is invalid"
        exit 126
    ;;
esac

exit 0;

# -------------------------------------------------------------------------------------------------------------------------- #
