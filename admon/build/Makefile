CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Admon,\
   $(sources),\
   common opentk dl sqlite openssl ticpp mongoose,\
   ../source)

$(call define-dep-lib,\
   Admon,\
   -I ../source,\
   $(build_dir)/libAdmon.a)

$(call build-executable,\
   beegfs-admon,\
   $(main),\
   Admon common opentk dl sqlite openssl ticpp mongoose)

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Admon common opentk dl sqlite openssl ticpp mongoose,\
   ../tests)

# JAVA_HOME will be honoured automatically by 'ant'

CXXFLAGS += -DTIXML_USE_TICPP

ANT_FINDBUGS_LIB ?= $(shell cd $(BEEGFS_THIRDPARTY_PATH) && pwd)/source/findbugs/lib/findbugs-ant.jar

ANT_INVOCATION := ant -lib $(ANT_FINDBUGS_LIB) -DBEEGFS_VERSION=$(BEEGFS_VERSION)

admon_gui:
	cd $(ADMON_GUI_PATH) && $(ANT_INVOCATION) clean || true
	cd $(ADMON_GUI_PATH) && $(ANT_INVOCATION)

define HELP_ARGS_SPECIFIC
   @echo '  JAVA_HOME=<path>           Path to Java JDK installation (optional)'
   @echo '  ADMON_GUI_PATH=<path>      Path to the admon_gui directory'
endef
