SHELL := /bin/bash

SOURCE_PATH     = $(shell echo `pwd`/../source )
BUILD_PATH      = $(shell echo `pwd` )

BUILDTYPE        = $(shell `pwd`/get-build-type.sh)

FINDBUGS_HOME    = $(shell echo `pwd`/../source/findbugs)

# note: cppunit not included in "all" by default, because we prefer system lib by default
all: gtest

GTEST_DIR = ../source/gtest/googletest
GTEST_SRC = $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc

libgtest.a: $(GTEST_SRC)
	$(CXX) -fPIC -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) -pthread -c $^
	$(AR) -r libgtest.a gtest-all.o gtest_main.o

gtest: libgtest.a

clean:
	rm -f gtest-all.o gtest_main.o libgtest.a
