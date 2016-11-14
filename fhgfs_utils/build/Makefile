# This is the beegfs_utils makefile.
#
# Use "make help" to find out about configuration options.

CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

all: beegfs_ctl beegfs_fsck beegfs_java_lib

export BEEGFS_COMMON_PATH
export BEEGFS_OPENTK_PATH
export BEEGFS_THIRDPARTY_PATH
export BEEGFS_VERSION
export BEEGFS_DEBUG

beegfs_ctl:
	$(MAKE) -C $(BEEGFS_ONLINE_CFG_PATH)/build
	cp $(BEEGFS_ONLINE_CFG_PATH)/build/beegfs-ctl .
	
beegfs_fsck:
	$(MAKE) -C $(BEEGFS_FSCK_PATH)/build
	cp $(BEEGFS_FSCK_PATH)/build/beegfs-fsck .

beegfs_java_lib:
	$(MAKE) -C $(BEEGFS_JAVA_LIB_PATH)/build
	cp $(BEEGFS_JAVA_LIB_PATH)/build/libjbeegfs.so $(BEEGFS_JAVA_LIB_PATH)/build/jbeegfs.jar .

clean: clean-siblings

clean-siblings:
	$(MAKE) -C $(BEEGFS_ONLINE_CFG_PATH)/build clean
	$(MAKE) -C $(BEEGFS_FSCK_PATH)/build clean
#	$(MAKE) -C $(BEEGFS_JAVA_LIB_PATH)/build clean
	
	rm -f beegfs-ctl
	rm -f beegfs-fsck
