CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lm -lraylib
SOURCES := src/linenoise.c src/config.c src/pixras.c src/main.c
OBJECTS = $(SOURCES:src/%.c=build/%.o)
TARGET = pixras

$(TARGET) : $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/%.o : src/%.c
	mkdir -p build && $(CC) $(CFLAGS) -c $^ -o $@

.PHONY: clean

clean:
	@rm -r build $(TARGET)
