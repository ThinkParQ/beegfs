# This is the beegfs_common makefile.
# It creates a static library.
#
# Use "make help" to find out about configuration options.

include $(or $(root-dir),../..)/build/Makefile


$(call build-static-library,\
   beegfs-common,\
   $(shell find ../source -iname '*.cpp'),\
   opentk,\
   ../source)

$(call build-static-library,\
   beegfs-tests,\
  ../tests/TestRunnerBase.cpp ../tests/Main.cpp,\
  common opentk)

$(call build-executable,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   common opentk cppunit tests,\
   ../tests)
