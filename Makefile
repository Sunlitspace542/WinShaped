OS := $(shell uname -s)
CC      ?= cc
RC ?= windres

CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic

LDLIBS  := -lm

TARGET  := dist/shaped

ifeq ($(OS),Linux)
OBJS := \
    build/main.o
else
TARGET := dist/shaped.exe
OBJS := \
    build/main.o \
    build/shaped.res.o
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p dist
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/main.o: src/main.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/shaped.res.o: src/shaped.rc
	@mkdir -p build
	$(RC) $< -O coff -o $@

clean:
	rm -rf build dist

format:
	@clang-format -i -style=file src/*.c
