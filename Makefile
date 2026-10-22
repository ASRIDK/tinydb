CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
# Required for getline(); kept separate so overriding CFLAGS cannot drop it.
override CPPFLAGS += -D_POSIX_C_SOURCE=200809L

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)
BIN := build/tinydb

.PHONY: all test sanitize clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p build

test: $(BIN)
	python3 -m unittest discover -s tests -v

# Rebuilds with AddressSanitizer and UndefinedBehaviorSanitizer, then runs the tests.
sanitize: clean
	$(MAKE) test CFLAGS="-std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined -fno-omit-frame-pointer"

clean:
	rm -rf build

-include $(OBJ:.o=.d)
