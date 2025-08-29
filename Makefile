CC=gcc
FLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=200112L -W -Wall -pedantic -Werror -std=c99
SOURCES = main-debug.c lib/*.c lib/utils/*.c 
HEADERS = lib/*.h lib/utils/*.h 

.PHONY: build install run lib

setup: 
	mkdir -p run && mkdir -p build build-debug

build_debug: FLAGS += -O2
build_debug: setup
	$(CC) $(FLAGS) $(SOURCES) -o ./build-debug/rld-debug-a

run_debug: build_debug
	./build-debug/rld-debug-a

build_debug_address: FLAGS += -fsanitize=undefined,address -g -D __DEBUG__
build_debug_address: setup
	$(CC) $(FLAGS) $(SOURCES) -o ./build-debug/rld-debug-a

run_debug_address: build_debug_address
	./build-debug/rld-debug-a --help

build_debug_thread: FLAGS += -fsanitize=undefined,thread -g -D __DEBUG__
build_debug_thread: setup
	$(CC) $(FLAGS) $(SOURCES) -o ./build-debug/rld-debug-t

run_debug_thread: build_debug_thread
	./build-debug/rld-debug-t
	
format: 
	clang-format -i $(SOURCES) $(HEADERS)

UTIL_INCLUDES = lib/utils/*.h 
ROOT_INCLUDES = lib/rld.h lib/helpers.h

build_lib: setup 
	$(CC) $(FLAGS) $(SOURCES) -fPIC -shared -o ./build/librld.so

build_script: setup
	cp rld.sh ./build/rld.sh && chmod +x ./build/rld.sh

build: build_script build_lib

clear_includes: 
	sudo rm -rf /usr/local/include/rld

install_includes: clear_includes
	sudo mkdir -p /usr/local/include/rld/utils && sudo cp $(ROOT_INCLUDES) /usr/local/include/rld/ && sudo cp $(UTIL_INCLUDES) /usr/local/include/rld/utils/

install_lib: install_includes build_lib
	sudo cp ./build/librld.so /usr/lib64

install_script: build_script
	sudo cp ./build/rld.sh /usr/local/bin/rld

install: install_lib install_script