# This is a mongoose makefile.
# It creates a static library.
# 
# Mongoose comes without a makefile in this release, so we wrote our own one
#


LIB = mongoose

CFLAGS = -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
	-pthread -rdynamic -Wall -fmessage-length=0 -fno-strict-aliasing \
	-Wunused-variable -Wextra -Wno-unused-parameter -ggdb3
CFLAGS_RELEASE=-O3
CFLAGS_DEBUG = -O0 -ggdb3 -D_FORTIFY_SOURCE=2 -DDEBUG \
	-DFHGFS_DEBUG -DDEBUG_READWRITE -DDEBUG_MUTEX_LOCKING -DDEBUG_REFCOUNT \
	-DDEBUG_BACKTRACE -DLOG_DEBUG_MESSAGES

CFLAGS += $(CFLAGS_RELEASE)

SOURCES = $(shell find . -name '*.c')

OBJECTS = $(SOURCES:.c=.o)
CLEANUP_OBJECTS = $(shell find . -name '*.o')

DEPENDENCY_FILES = $(shell find . -name '*.d')

all: $(SOURCES) $(LIB)

$(LIB): $(OBJECTS)
	ar -r lib$(LIB).a $(OBJECTS)

.c.o: 
	$(CC) $(CFLAGS) -c $(@:.o=.c) -E -MMD -MF$(@:.o=.d) -MT$(@) -o/dev/null
	$(CC) $(CFLAGS) -o$(@) -c $(@:.o=.c) 


clean:
	rm -rf $(CLEANUP_OBJECTS) $(DEPENDENCY_FILES) lib$(LIB).a

help:
	@echo 'Optional Arguments:'
	@echo '- CXX=<compiler>:'
	@echo '    Specifies a c++ compiler.'
