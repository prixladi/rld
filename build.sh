#! /bin/env bash

set -euo pipefail

SRC_ARCHIVE=$(tar -cf - -C src . | base64 --wrap=0)
SCRIPT=$(cat ./rld-template.sh)

readarray -t SCRIPT_LINES <<<"$SCRIPT"

printf "" > ./build/rld.sh
for line in "${SCRIPT_LINES[@]}"
do
    if  [[ "$line" == "    #<<<SRC_ARCHIVE>>>" ]]
    then
        echo "SRC_ARCHIVE=$SRC_ARCHIVE" >> "./build/rld.sh"
    else
        echo "$line" >> "./build/rld.sh"
    fi
done