# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pedantic

# Source files
SRC = edie.c

# Normal build (default target)
default: ei

ei: $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

# Slim build
slim: $(SRC)
	echo 'you need gcc to slim build!'
	gcc -Os -s -fno-stack-protector -fno-ident     -ffunction-sections -fdata-sections     -Wl,--gc-sections     -Wl,-z,norelro -Wl,--build-id=none     edie.c -o ei

# Static build
static: $(SRC)
	$(CC) $(CFLAGS) -Os -static -o ei $^

# Install build
.PHONY: install
install: ei
	install -m 777 ei /usr/bin/

# Clean up
.PHONY: clean
clean:
	 rm -f ei slim static
