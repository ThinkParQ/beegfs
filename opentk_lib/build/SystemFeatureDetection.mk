# All detected features are included in "SYSTEM_FEATURE_DETECTION"

# find ofed include path
# (we check for dirs that contain "rdma/rdma_cma.h" here)
ifneq ($(OFED_INCLUDE_PATH),)
feature_test_ofed_dirs = $(OFED_INCLUDE_PATH)
endif

feature_test_ofed_dirs += /usr/local/include
feature_test_ofed_dirs += /usr/include

test_dir = $(shell [ -e $(dir)/rdma/rdma_cma.h ] && echo $(dir) )
feature_test_ofed_dirs_pruned := $(foreach dir, $(feature_test_ofed_dirs), $(test_dir) )

feature_test_ofed_dir := $(firstword $(feature_test_ofed_dirs_pruned) )


# SYSTEM_HAS_RDMA_MIGRATE_ID Detection [START]
#
# Find out whether we are using an OFED version with rdma_migrate_id support.
# Note: rdma_migrate_id() is not available in OFED 1.2.
# Note: Test just greps for rdma_migrate_id (so it might result in false positives,
#       but that is unlikely).

ifneq ($(BEEGFS_OPENTK_IBVERBS),)

SYSTEM_HAS_RDMA_MIGRATE_ID = $(shell \
	if grep -s rdma_migrate_id ${feature_test_ofed_dir}/rdma/rdma_cma.h 1>/dev/null 2>&1 ; \
	then echo "-DSYSTEM_HAS_RDMA_MIGRATE_ID" ; \
	fi)

endif

#
# SYSTEM_HAS_RDMA_MIGRATE_ID Detection [END]

# SYSTEM_HAS_RDMA_CM_EVENT_TIMEWAIT_EXIT Detection [START]
#
# Find out whether we are using an OFED version that has
# RDMA_CM_EVENT_TIMEWAIT_EXIT enum defined.
# Note: RDMA_CM_EVENT_TIMEWAIT_EXIT is not available in SLES10.

ifneq ($(BEEGFS_OPENTK_IBVERBS),)

SYSTEM_HAS_RDMA_CM_EVENT_TIMEWAIT_EXIT = $(shell \
	if grep -s RDMA_CM_EVENT_TIMEWAIT_EXIT \
		${feature_test_ofed_dir}/rdma/rdma_cma.h 1>/dev/null 2>&1 ; \
	then echo "-DSYSTEM_HAS_RDMA_CM_EVENT_TIMEWAIT_EXIT" ; \
	fi)

endif

#
# SYSTEM_HAS_RDMA_MIGRATE_ID Detection [END]


# Combine results
SYSTEM_FEATURE_DETECTION := $(SYSTEM_HAS_RDMA_MIGRATE_ID) \
	$(SYSTEM_HAS_RDMA_CM_EVENT_TIMEWAIT_EXIT)
