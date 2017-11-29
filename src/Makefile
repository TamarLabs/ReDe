
# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')
export CC

# if DEBUG env var is set, we compile with "debug" cflags
DEBUGFLAGS = -g -ggdb -O3
ifeq ($(DEBUG), 1)
	DEBUGFLAGS = -g -ggdb -O0
endif

# Default CFLAGS
CFLAGS= -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-result -fPIC \
 	-D_GNU_SOURCE -std=gnu99 -I"$(shell pwd)" -DREDISMODULE_EXPERIMENTAL_API 
CFLAGS += $(DEBUGFLAGS)

# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	SHOBJ_LDFLAGS ?= -shared -Bsymbolic -Bsymbolic-functions -ldl -lpthread
else
	CFLAGS += -mmacosx-version-min=10.6
	SHOBJ_LDFLAGS ?= -macosx_version_min 10.6 -exported_symbol _RedisModule_OnLoad -bundle -undefined dynamic_lookup -ldl -lpthread
endif
export CFLAGS



# Sources
SOURCEDIR=$(shell pwd -P)
CC_SOURCES = $(wildcard $(SOURCEDIR)/*.c)
CC_SOURCES += $(wildcard $(SOURCEDIR)/util/*.c)
CC_SOURCES += $(wildcard $(SOURCEDIR)/trie/*.c)

# Convert all sources to .o files
CC_OBJECTS = $(patsubst %.c, %.o, $(CC_SOURCES) )

# .d files for each c file. These make sure that changing a header file
# will also change the dependent .c files of it
CC_DEPS = $(patsubst %.c, %.d, $(CC_SOURCES) )

# Library dependencies
LIBRMUTIL=rmutil/librmutil.a
LIBTRIEMAP=trie/libtriemap.a

# Compilation deps for the module
LIBS= $(LIBTRIEMAP) #$(LIBRMUTIL) $(LIBTRIE) $(LIBNU) $(LIBSTEMMER)
MODULE=$(CC_OBJECTS) $(LIBS)

%.c: %.y

# Compile C file while generating a .d file for it
%.o: %.c
%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@ -MMD -MF $(@:.o=.d)

all: dehy.so module.so

# Include all dependency files for C files
-include $(CC_DEPS)

# Library compile rules
$(LIBRMUTIL):
	$(MAKE) -C rmutil
.PHONY: $(LIBRMUTIL)
$(LIBTRIE):
	$(MAKE) -C trie
.PHONY: $(LIBTRIE)
$(LIBTRIEMAP):
	$(MAKE) -C trie 
.PHONY: $(LIBTRIEMAP)
$(LIBSTEMMER):
	$(MAKE) -C dep/snowball libstemmer.o
.PHONY: $(LIBSTEMMER)
$(LIBNU):
	$(MAKE) -C dep/libnu
.PHONY: $(LIBNU)


build_tests: dehy.so module.so 
	$(MAKE) -C tests build

test: $(MODULE) build_tests
	# low level redis-independant tests
	# ./tests/test
	$(MAKE) -C ./tests test
	# high level python integration tests
	# $(MAKE) -C pytest test

buildall:  dehy.so module.so build_tests

# Build the module...
module.so: $(MODULE) # version.h
	# Just to make sure old versions of the modules are deleted
	rm -f module.so
	$(LD) -o $@ $(MODULE) $(SHOBJ_LDFLAGS) -lc -lm 

# Build the lib...
dehy.so: $(MODULE)
	# Just to make sure old versions of the lib are deleted
	rm -f dehy.so
	$(LD) -o $@ $(MODULE) $(SHOBJ_LDFLAGS) -lc -lm 

# Build a stand-alone static library without the module entry point.
# This is used to include the module's functionality in other modules
libdehy.a: CFLAGS+=-DRS_CMD_PREFIX='"_FT"' -DRS_STATIC
libdehy.a: $(CC_OBJECTS) $(LIBS) # version.h
	ar rcs $@ $(CC_OBJECTS)

staticlib: libdehy.a

clean:
	rm -fv *.[oad] util/*.[oad] trie/*.[oad] tests/*.[oad] tests/*.run rmutil/*.[oad]
	$(MAKE) -C tests clean

distclean:
	find . -type f \( -name '*.[oad]' -o -name '*.so' \) -delete -print

package: dehy.so
	mkdir -p ../build
	module_packer -v -o "../build/dehy-oss.{os}-{architecture}.latest.zip" "`pwd`/dehy.so"

# print_version: version.h print_version.c
# 	@$(CC) -o $@ -DPRINT_VERSION_TARGET $@.c

rebuild: clean all
