# At the moment there's not much more to do than moving the GNU Parallel script to the correct
# directory.

SHELL := /bin/bash

SOURCE_PATH = $(shell echo `pwd`/../source )

PARALLEL_VER         = 20150122
PARALLEL_PATH        = $(SOURCE_PATH)/parallel-$(PARALLEL_VER)
PARALLEL_SCRIPT_IN   = $(PARALLEL_PATH)/src/parallel
PARALLEL_SCRIPT_OUT  = ./parallel
PARALLEL_COPYING_IN  = $(PARALLEL_PATH)/COPYING
PARALLEL_COPYING_OUT = ./COPYING

# Note: Although parallel provides a makefile, that is mainly used for packaging. We just want
# to run it, and we only need the main 'parallel' script, without the supplementary tools it
# provides, so we can take the script directly from the source directory.

.PHONY: all clean rpm deb

all: $(PARALLEL_SCRIPT_OUT) $(PARALLEL_COPYING_OUT)

$(PARALLEL_SCRIPT_OUT): $(PARALLEL_SCRIPT_IN)
	cp $(PARALLEL_SCRIPT_IN) $(PARALLEL_SCRIPT_OUT)

$(PARALLEL_COPYING_OUT): $(PARALLEL_COPYING_IN)
	cp $(PARALLEL_COPYING_IN) $(PARALLEL_COPYING_OUT)


# Note: This breaks if someone selectively deletes the parallel or COPYING files from the parallel
#       source directory.
$(PARALLEL_SCRIPT_IN): | $(PARALLEL_PATH)

$(PARALLEL_COPYING_IN): | $(PARALLEL_PATH)

$(PARALLEL_PATH):
	cd $(SOURCE_PATH) && \
	tar xvf parallel-$(PARALLEL_VER).tar.bz2

clean:
	-rm -f $(PARALLEL_SCRIPT_OUT)
	-rm -f $(PARALLEL_COPYING_OUT)
	-rm -fr $(PARALLEL_PATH)

