include ../../build/Makefile

TARGETS = ctl fsck java_lib event_listener

all: $(TARGETS)

export BEEGFS_COMMON_PATH
export BEEGFS_THIRDPARTY_PATH
export BEEGFS_EVENT_LISTENER_PATH
export BEEGFS_VERSION
export BEEGFS_DEBUG

ctl:
	$(MAKE) -C $(BEEGFS_ONLINE_CFG_PATH)/build
	cp $(BEEGFS_ONLINE_CFG_PATH)/build/beegfs-ctl .
	
fsck:
	$(MAKE) -C $(BEEGFS_FSCK_PATH)/build
	cp $(BEEGFS_FSCK_PATH)/build/beegfs-fsck .

java_lib:
	$(MAKE) -C $(BEEGFS_JAVA_LIB_PATH)/build
	cp $(BEEGFS_JAVA_LIB_PATH)/build/libjbeegfs.so $(BEEGFS_JAVA_LIB_PATH)/build/jbeegfs.jar .

event_listener:
	$(MAKE) -C $(BEEGFS_EVENT_LISTENER_PATH)/build
	cp $(BEEGFS_EVENT_LISTENER_PATH)/build/beegfs-event-listener .

clean: clean-siblings

clean-siblings:
	$(MAKE) -C $(BEEGFS_ONLINE_CFG_PATH)/build clean
	$(MAKE) -C $(BEEGFS_FSCK_PATH)/build clean
	$(MAKE) -C $(BEEGFS_EVENT_LISTENER_PATH)/build clean
#	$(MAKE) -C $(BEEGFS_JAVA_LIB_PATH)/build clean
	
	rm -f beegfs-ctl
	rm -f beegfs-fsck
	rm -f beegfs-event-listener
