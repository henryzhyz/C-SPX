CC=gcc
CFLAGS=-Wall -Werror -Wvla -O0 -std=c11 -g -fsanitize=address,leak
#CFLAGS = -Wall -Werror -Wvla -O0 -std=c11 -g
LDFLAGS=-lm
BINARIES=spx_exchange spx_trader

all: $(BINARIES)

.PHONY: clean
clean:
	rm -f $(BINARIES)

