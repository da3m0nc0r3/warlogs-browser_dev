# Makefile for warlogs-browser (ncurses + sqlite3)
# Assumes: sources in src/, headers in include/, objects in obj/

TARGET  := bin/warlogs-browser
CC      := gcc
CFLAGS  := -Wall -Wextra -pedantic -std=c11 -g
LDFLAGS :=
LDLIBS  := -lncurses -lsqlite3

# Find sources
SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c, obj/%.o, $(SOURCES))
DEPFILES := $(patsubst src/%.c, obj/%.d, $(SOURCES))

# Add include path
CFLAGS := -Wall -Wextra -pedantic -std=c11 -g -D_GNU_SOURCE

# === Main targets ===
.PHONY: all run clean debug pedantic

all: $(TARGET)

$(TARGET): $(OBJECTS) | bin
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(LDLIBS)

bin:
	mkdir -p bin

obj:
	mkdir -p obj

# Pattern rule → compile .c from src/ → .o in obj/
obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Include auto-generated dependency files
-include $(DEPFILES)

run: $(TARGET)
	@$(TARGET)   # @ = don't print command before executing

debug: CFLAGS += -Og -ggdb3
debug: clean all
	@echo "Debug build ready. You can now run:"
	@echo "  gdb $(TARGET)"
	@echo "  or"
	@echo "  lldb $(TARGET)"

clean:
	$(RM) $(TARGET) $(OBJECTS) $(DEPFILES)
	$(RM) -r bin obj
	clear

pedantic: CFLAGS += -Werror -Wshadow -Wconversion -Wsign-conversion
pedantic: clean all