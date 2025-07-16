CC=gcc
FLAGS = -W -std=c99 -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -O2
SOURCES = main.c lib/*.c lib/utils/*.c 

.PHONY: build

setup: 
	mkdir -p .bin

build: setup
	$(CC) $(FLAGS) $(SOURCES) -o ./.bin/rld

run: build
	cd .. && ./.rld/.bin/rld