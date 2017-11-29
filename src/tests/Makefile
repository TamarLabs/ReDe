ifndef RM_INCLUDE_DIR
	RM_INCLUDE_DIR=../
endif

CFLAGS = -g -O2 -fPIC -std=gnu99 -I./ 
CFLAGS += -I$(RM_INCLUDE_DIR)
%.c: %.y

ifndef VERBOSE
.SILENT:
endif

# Sources
SOURCEDIR=..
CC_SOURCES = $(wildcard $(SOURCEDIR)/*.c)
CC_SOURCES += $(wildcard $(SOURCEDIR)/util/*.c)
CC_SOURCES += $(wildcard $(SOURCEDIR)/trie/*.c)

# Convert all sources to .o files
DEP_OBJECTS = $(patsubst %.c, %.o, $(CC_SOURCES) )
CC_DEPS = $(patsubst %.c, %.d, $(CC_SOURCES) )


TEST_SOURCES = $(wildcard test_*.c)
TEST_OBJECTS = $(patsubst %.c, %.o, $(TEST_SOURCES)  )
TEST_EXECUTABLES = $(patsubst %.c, %.run, $(TEST_SOURCES)  )
TEST_DEPS = $(patsubst %.c, %.d,$(TEST_SOURCES))

# Library dependencies
DEP_LIBS =  ../trie/libtriemap.a #../rmutil/librmutil.a
DEPS = $(DEP_OBJECTS) $(DEP_LIBS)
SRCDIR := $(shell pwd)
LDFLAGS :=  -lc -lm -ldl -lpthread 

CC=gcc

%.o: %.c
%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@ -MMD -MF $(@:.o=.d)


all: build test

# Including this stuff makes sure everything gets built on changes to the source directory
-include $(TEST_DEPS)
-include $(CC_DEPS)


# Compiling each test runner from its .o file
%.run: %.o
	$(CC) $(CFLAGS) -o $@ $^  $(DEPS) $(LDFLAGS)

build: $(TEST_OBJECTS) $(TEST_EXECUTABLES) $(DEPS)

# Test all
test: build $(TEST_EXECUTABLES)
	set -e; \
	for t in test_*.run;\
	 do ./$$t;\
	done
	# ./tests/helloworld.py
	# ./tests/test.py --noload

memcheck: build $(TEST_EXECUTABLES)
	set -e; \
	for t in test_*.run;\
	 do valgrind --tool=memcheck --leak-check=full --error-exitcode=1 --show-possibly-lost=no ./$$t;\
	done

# Target for individual tests - make run:{test name}, e.g. "make run:somthing" will run test_somthing.run
run\:%:
	$(MAKE) test_$*.run
	./test_$*.run

clean:
	-rm -f *.o

.PHONY: clean

rebuild: clean all
