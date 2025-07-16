CC=gcc
FLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=200112L -W -Wall -pedantic -Werror -std=c99 -Wno-gnu-auto-type
SOURCES = main-debug.c src/lib/*.c src/lib/utils/*.c 
HEADERS = src/lib/*.h src/lib/utils/*.h 

.PHONY: build install run

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
	./build-debug/rld-debug-a

build_debug_thread: FLAGS += -fsanitize=undefined,thread -g -D __DEBUG__
build_debug_thread: setup
	$(CC) $(FLAGS) $(SOURCES) -o ./build-debug/rld-debug-t

run_debug_thread: build_debug_thread
	./build-debug/rld-debug-t
	
format: 
	clang-format -i $(SOURCES) $(HEADERS)

build: setup
	./build.sh

install: build
	sudo cp ./build/rld.sh /usr/local/bin/rld && sudo chmod +x /usr/local/bin/rld