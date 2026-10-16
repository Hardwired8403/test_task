CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
LDFLAGS ?=

.PHONY: all test clean

all: dump_tool test_dump

dump_tool: dump_tool.o statdata.o
	$(CC) $(LDFLAGS) -o $@ $^

test_dump: test_dump.o statdata.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c statdata.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: all
	./test_dump ./dump_tool

clean:
	rm -f dump_tool test_dump *.o
