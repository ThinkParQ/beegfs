include ../../build/Makefile


$(call build-static-library,\
   beegfs-common,\
   $(shell find ../source -iname '*.cpp'),\
   dl nl3-route,\
   ../source)

$(call build-shared-library,\
   beegfs_ib,\
   $(shell find ../ib_lib -iname '*.cpp'),\
   rdma,\
   ../source ../ib_lib)


$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   common,\
   ../tests)
