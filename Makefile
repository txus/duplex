CC=clang
CFLAGS=-g -O3 -Isrc $(OPTFLAGS)
LIBS=$(OPTLIBS)

PREFIX?=/usr/local

SOURCES=$(wildcard src/**/*.c src/*.c)
OBJECTS=$(patsubst %.c,%.o,$(SOURCES))

TEST_SRC=$(wildcard tests/*_tests.c)
TESTS=$(patsubst %.c,%,$(TEST_SRC))

PROGRAMS_SRC=$(wildcard bin/*.c)
PROGRAMS=$(patsubst %.c,%,$(PROGRAMS_SRC))

TARGET=build/libduplex.a
SO_TARGET=$(patsubst %.a,%.so,$(TARGET))

# The Target Build
all: $(TARGET) $(PROGRAMS) tests

dev: CFLAGS=-g -std=c99 -Wall -Isrc $(OPTFLAGS)
dev: all

$(TARGET): $(LIBS)
$(TARGET): build $(OBJECTS)
				ar rcs $@ $(OBJECTS)
				ranlib $@

$(PROGRAMS): bin/%: bin/%.c
				$(CC) $(CFLAGS) $< -o $@ $(TARGET) $(LDFLAGS)

$(SO_TARGET): $(TARGET) $(OBJECTS)
				$(CC) -shared -o $@ $(OBJECTS)

build:
				@mkdir -p build
				@mkdir -p bin

# The Unit Tests
.PHONY: tests
$(TESTS): tests/%: tests/%.c
				$(CC) $(CFLAGS) $< -o $@ $(TARGET) $(LDFLAGS)
tests: $(TESTS)
				sh ./tests/runtests.sh

valgrind:
				VALGRIND="valgrind" $(MAKE)

# The Cleaner
clean:
				rm -rf build $(OBJECTS) $(TESTS) $(PROGRAMS)
				find . -name "*.gc*" -exec rm {} \;
				rm -rf `find . -name "*.dSYM" -print`

# The Install
install: all
				install -d $(DESTDIR)/$(PREFIX)/lib/
				install $(TARGET) $(DESTDIR)/$(PREFIX)/lib/
				install $(PROGRAMS) $(DESTDIR)/$(PREFIX)/bin/

# The Checker
BADFUNCS='[^_.>a-zA-Z0-9](str(n?cpy|n?cat|xfrm|n?dup|str|pbrk|tok|_)|stpn?cpy|a?sn?printf|byte_)'
check:
				@echo Files with potentially dangerous functions.
				@egrep $(BADFUNCS) $(SOURCES) || true

