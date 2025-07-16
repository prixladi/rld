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

ensure_rld_exists () {
    if [ ! -d "./$RLD_DIR" ] || [ -z "$( ls -A "./$RLD_DIR" )" ]; then
        echo "Rld is not initialized in the current directory, the '$RLD_DIR' directory does exists or it is empty!"
        exit 4;
    fi
}

ensure_rld_does_not_exist () {
    if [ -d "./$RLD_DIR" ] && [ ! -z "$( ls -A "./$RLD_DIR" )" ]; then
        echo "Rld is already initialized in the current directory, the '$RLD_DIR' directory exists and it is not empty!"
        exit 9;
    fi
}

# -------------------------------------------------------------------------------------------------------------------------- #

rld_initialize () {
    mkdir -p $RLD_DIR
    
    cd $RLD_DIR
    
    #<<<SRC_ARCHIVE>>>
    
    echo $SRC_ARCHIVE | base64 -d > "./src.tar"
    
    tar -xvf "./src.tar" > /dev/null
    mv .gitignore.template .gitignore
    
    rm "./src.tar"
    
    cd ..
}

rld_run () {
    cd $RLD_DIR
    make run
}

rld_update () {
    if [ ! -f "./$RLD_DIR/main.c" ]; then
        echo "Main.c does not exist in '$RLD_DIR'"
        exit 10;
    fi
    
    mv ./$RLD_DIR/main.c ./$RLD_DIR/main-backup.c
    rm -rf ./$RLD_DIR/lib ./$RLD_DIR/Makefile ./$RLD_DIR/.gitignore
    
    rld_initialize
    mv ./$RLD_DIR/main-backup.c ./$RLD_DIR/main.c
}

# -------------------------------------------------------------------------------------------------------------------------- #

case $1 in
    
    "init")
        ensure_rld_does_not_exist
        rld_initialize
    ;;
    
    "run")
        ensure_rld_exists
        rld_run
    ;;
    
    "update")
        ensure_rld_exists
        rld_update
    ;;
    
    *)
        echo "Command $1 is invalid"
        exit 126
    ;;
esac

exit 0;

# -------------------------------------------------------------------------------------------------------------------------- #
