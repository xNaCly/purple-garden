FLAGS := -std=c2x \
        -g3 \
        -O3 \
        -Wall \
        -Wextra \
        -Werror \
        -fdiagnostics-color=always \
        -fsanitize=address,undefined \
        -fno-common \
        -Winit-self \
        -Wfloat-equal \
        -Wundef \
        -Wshadow \
        -Wpointer-arith \
        -Wcast-align \
        -Wstrict-prototypes \
        -Wstrict-overflow=5 \
        -Wwrite-strings \
        -Waggregate-return \
        -Wcast-qual \
        -Wswitch-default \
        -Wunreachable-code \
        -Wno-discarded-qualifiers \
		-Wno-unused-parameter \
		-Wno-unused-function \
		-Wno-aggregate-return

COMMIT := $(shell git rev-parse --short HEAD)
FILES := $(shell find . -maxdepth 1 -name "*.c" ! -name "main.c")
TEST_FILES := $(shell find ./tests -name "*.c")
PG := ./examples/hello-world.garden

.PHONY: run build test clean bench

bench:
	$(CC) $(FLAGS) -DCOMMIT='"BENCH"' -DBENCH=1 $(FILES) ./main.c -o bench
	./bench $(PG)

run: build
	./purple_garden $(PG)

test:
	$(CC) $(FLAGS) $(TEST_FILES) $(FILES) -DDEBUG=1 -o ./tests/test
	./tests/test

build:
	$(CC) $(FLAGS) -DCOMMIT='"$(COMMIT)"' $(FILES) ./main.c -o purple_garden

clean:
	rm -fv ./purple_garden ./tests/test ./bench
