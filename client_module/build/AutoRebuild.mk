# Automatic rebuild of BeeGFS client modules on kernel change.


AUTO_REBUILD_KVER_FILE := auto_rebuild_kernel.ver
AUTO_REBUILD_KVER_CURRENT = $(shell uname -srvmpi)

AUTO_REBUILD_LOG_PREFIX := "BeeGFS Client Auto-Rebuild:"

# config file (keys)
AUTO_REBUILD_CONF_FILE := /etc/beegfs/beegfs-client-autobuild.conf
AUTO_REBUILD_CONF_ENABLED_KEY := buildEnabled
AUTO_REBUILD_CONF_BUILDARGS_KEY := buildArgs

# config file (values)
AUTO_REBUILD_CONF_ENABLED = $(shell \
	grep -s "^\s*$(AUTO_REBUILD_CONF_ENABLED_KEY)\s*=" "$(AUTO_REBUILD_CONF_FILE)" | \
	cut --delimiter="=" --fields="2-" )
AUTO_REBUILD_CONF_BUILDARGS = $(shell \
	grep "^\s*$(AUTO_REBUILD_CONF_BUILDARGS_KEY)\s*=" "$(AUTO_REBUILD_CONF_FILE)" | \
	cut --delimiter="=" --fields="2-" )

# add build dependency if rebuild has been enabled in config file
ifeq ($(AUTO_REBUILD_CONF_ENABLED), true)
AUTO_REBUILD_CONFIGURED_DEPS := auto_rebuild_install
endif

# environment variables (intentionally commented out here; just to mention
# them somewhere)
# - AUTO_REBUILD_KVER_STORED: internally for target auto_rebuild


auto_rebuild:
	$(MAKE) auto_rebuild_clean $(AUTO_REBUILD_CONF_BUILDARGS)
	$(MAKE) $(AUTO_REBUILD_CONF_BUILDARGS)

# checked rebuild and install
auto_rebuild_install: auto_rebuild
	$(MAKE) install $(AUTO_REBUILD_CONF_BUILDARGS)


# run checked rebuild and install if enabled in config file
auto_rebuild_configured: $(AUTO_REBUILD_CONFIGURED_DEPS)
	@ /bin/true


auto_rebuild_clean: clean
	@ /bin/true


auto_rebuild_help:	
	@echo 'No Auto-Rebuild Arguments defined.'

