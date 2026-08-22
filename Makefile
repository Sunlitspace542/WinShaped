CC      := gcc
WINDRES := windres

CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic \
           -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN

LDFLAGS := -static -static-libgcc -municode -mwindows
LDLIBS  := -lgdi32 -luser32

TARGET  := dist/Shaped.exe

OBJS := \
    build/main.o \
    build/shaped.res.o

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
	$(WINDRES) $< -O coff -o $@

clean:
	rm -rf build dist

format:
	@clang-format -i -style=file src/*.c
