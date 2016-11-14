# This is the beegfs_admon makefile.
# It creates an ELF executable.
# 
# Use "make help" to find out about configuration options.

CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

$(call build-executable,\
   beegfs-admon,\
   $(shell find ../source -iname '*.cpp'),\
   common opentk cppunit dl sqlite openssl ticpp mongoose)

# JAVA_HOME will be honoured automatically by 'ant'

CXXFLAGS += -DTIXML_USE_TICPP

ANT_FINDBUGS_LIB ?= $(shell cd $(BEEGFS_THIRDPARTY_PATH) && pwd)/source/findbugs/lib/findbugs-ant.jar

ANT_INVOCATION := ant -lib $(ANT_FINDBUGS_LIB) -DBEEGFS_VERSION=$(BEEGFS_VERSION)

admon_gui:
	cd $(ADMON_GUI_PATH) && $(ANT_INVOCATION) clean || true
	cd $(ADMON_GUI_PATH) && $(ANT_INVOCATION)

define HELP_ARGS_SPECIFIC
   @echo '  JAVA_HOME=<path>           Path to Java JDK installation (optional)'
   @echo '  ADMON_GUI_PATH=<path>      Path to the beegfs_admon_gui directory'
endef
