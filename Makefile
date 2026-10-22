CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
CFLAGS  += -D_POSIX_C_SOURCE=200809L

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)
BIN := build/tinydb

.PHONY: all test clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p build

test: $(BIN)
	python3 -m unittest discover -s tests -v

clean:
	rm -rf build

-include $(OBJ:.o=.d)
