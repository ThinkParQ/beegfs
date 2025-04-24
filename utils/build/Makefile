include ../../build/Makefile

TARGETS = fsck event_listener

all: $(TARGETS)

export BEEGFS_COMMON_PATH
export BEEGFS_THIRDPARTY_PATH
export BEEGFS_EVENT_LISTENER_PATH
export BEEGFS_VERSION
export BEEGFS_DEBUG
	
fsck:
	$(MAKE) -C $(BEEGFS_FSCK_PATH)/build
	cp $(BEEGFS_FSCK_PATH)/build/beegfs-fsck .

event_listener:
	$(MAKE) -C $(BEEGFS_EVENT_LISTENER_PATH)/build
	cp $(BEEGFS_EVENT_LISTENER_PATH)/build/beegfs-event-listener .

clean: clean-siblings

clean-siblings:
	$(MAKE) -C $(BEEGFS_ONLINE_CFG_PATH)/build clean
	$(MAKE) -C $(BEEGFS_FSCK_PATH)/build clean
	$(MAKE) -C $(BEEGFS_EVENT_LISTENER_PATH)/build clean
	
	rm -f beegfs-fsck
	rm -f beegfs-event-listener
