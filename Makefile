# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pedantic

# Source files
SRC = main.c

# Normal build (default target)
default: edie

edie: $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

# Slim build
slim: $(SRC)
	$(CC) -Os -o $@ $^

# Static build
static: $(SRC)
	$(CC) $(CFLAGS) -Os -static -o $@ $^

# Clean up
.PHONY: clean
clean:
	 rm -f edie slim static
