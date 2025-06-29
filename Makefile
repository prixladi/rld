CC=gcc
FLAGS = -W -std=c99
SOURCES = src/*.c src/lib/*.c src/lib/utils/*.c 
HEADERS = src/*.h src/lib/*.h src/lib/utils/*.h 

.PHONY: build install run

setup: 
	mkdir -p run && mkdir -p build

build: FLAGS += -O2
build: setup
	$(CC) $(FLAGS) $(SOURCES) -o ./build/reload

run: build
	./build/reload -w ./run

build_debug_address: FLAGS += -fsanitize=undefined,address -g -D __DEBUG__
build_debug_address: setup
	$(CC) $(FLAGS) $(SOURCES) -o ./build/reload-debug

run_debug_address: build_debug_address
	./build/reload-debug -w ./run