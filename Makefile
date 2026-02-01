# =========================
#   EDIE Makefile (glibc/musl)
# =========================

# Build mode: glibc (default) or musl
MODE ?= glibc

# Compiler selection
ifeq ($(MODE),musl)
    CC := musl-gcc
else
    CC := gcc
endif

# Common flags
CFLAGS  := -Wall -Wextra -pedantic
SRC     := edie.c
BIN     := ei

# Default build
default: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -O2 -o $@ $^

# Slim build (small dynamic binary)
slim:
	$(CC) -Os -s \
		-fno-stack-protector -fno-ident \
		-ffunction-sections -fdata-sections \
		-Wl,--gc-sections -Wl,-z,norelro -Wl,--build-id=none \
		$(SRC) -o $(BIN)

# Static build
static:
	$(CC) $(CFLAGS) -Os -static $(SRC) -o $(BIN)

# Install
.PHONY: install
install: $(BIN)
	install -m 0755 $(BIN) /usr/bin/

# Clean
.PHONY: clean
clean:
	rm -f $(BIN)

.PHONY: compress
# Compressed build (strip + optional UPX)
compress: $(BIN)
	@echo "[*] Stripping binary..."
	@strip $(BIN) || true

	@if command -v upx >/dev/null 2>&1; then \
		echo "[*] UPX found, compressing..."; \
		upx --best --lzma $(BIN); \
	else \
		echo "[*] UPX not found, skipping compression."; \
	fi

	@echo "[*] Final size:"
	@ls -lh $(BIN)
