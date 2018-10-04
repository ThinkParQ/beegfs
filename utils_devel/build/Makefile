BEEGFS_EVENT_LISTENER_PATH ?= ../../event_listener

ifneq ($(BEEGFS_VERSION),)
BEEGFS_EXTRA_FLAGS += 'BEEGFS_VERSION="$(BEEGFS_VERSION)"'
endif

ifneq ($(BEEGFS_DEBUG),)
BEEGFS_EXTRA_FLAGS += 'BEEGFS_DEBUG=$(BEEGFS_DEBUG)'
endif


all:

clean:

help:
	@echo 'Optional Arguments:'
	@echo '   BEEGFS_DEBUG=1:'
	@echo '	     Enables debug information and symbols.'
	@echo '   CXX=<compiler>:'
	@echo '      Specifies a c++ compiler.'
	@echo '   BEEGFS_COMMON_PATH=<path>:'
	@echo '      Path to the common directory.'
	@echo
	@echo 'Targets:'
	@echo '   all (default)     - build only'
	@echo '   help              - print this help message'


# Include dependency files
ifneq ($(DEPENDENCY_FILES),)
include $(DEPENDENCY_FILES)
endif

