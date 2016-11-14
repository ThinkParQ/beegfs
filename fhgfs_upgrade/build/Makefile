# This is the fhgfs_utils makefile.
# 
# Use "make help" to find out about configuration options.


FHGFS_UPGRADE_META_2011_PATH = ../fhgfs_upgrade_meta_to_2011.04
FHGFS_UPGRADE_META_2012_PATH = ../fhgfs_upgrade_meta_to_2012.10
FHGFS_UPGRADE_STORAGE_2011_PATH = ../fhgfs_upgrade_storage_to_2011.04
FHGFS_UPGRADE_STORAGE_2012_PATH = ../fhgfs_upgrade_storage_to_2012.10

ifneq ($(FHGFS_VERSION),)
FHGFS_EXTRA_FLAGS += 'FHGFS_VERSION=$(FHGFS_VERSION)'
endif

ifneq ($(FHGFS_DEBUG),)
FHGFS_EXTRA_FLAGS += 'FHGFS_DEBUG=$(FHGFS_DEBUG)'
endif


all: rpm

clean: 
	make -C $(FHGFS_UPGRADE_META_2011_PATH)/build clean
	make -C $(FHGFS_UPGRADE_META_2012_PATH)/build clean
	make -C $(FHGFS_UPGRADE_STORAGE_2011_PATH)/build clean
	make -C $(FHGFS_UPGRADE_STORAGE_2012_PATH)/build clean
	
rpm:
	# ./make-rpm2011, we don't enable the 2011 upgrade tool, it is not needed
	# and also creates wrong package names in the 2012 branch and master
	./make-rpm2012

deb:
	./make-deb

help:
	@echo 'Optional Arguments:'
	@echo '   FHGFS_DEBUG=1:'
	@echo '	     Enables debug information and symbols.'
	@echo '   CXX=<compiler>:'
	@echo '      Specifies a c++ compiler.'
	@echo 'Targets:'
	@echo '   rpm               - create a rpm file and write it to buildroot/RPMS/'
	@echo '   deb               - create debian package file'
	@echo '   help              - print this help message'


