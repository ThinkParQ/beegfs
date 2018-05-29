include $(or $(root-dir),../..)/build/Makefile


$(call build-static-library,\
   beegfs-common,\
   $(shell find ../source -iname '*.cpp'),\
   opentk,\
   ../source)


$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   common opentk,\
   ../tests)
