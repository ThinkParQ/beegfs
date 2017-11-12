# This is the fhgfs_thirdparty makefile.
# It creates needed thirdparty libs.

SHELL := /bin/bash

ifneq ($(target_arch),)
   configure_host := --host=$(target_arch)
   make_host := CC=$(target_arch)-gcc CXX=$(target_arch)-g++ AR=$(target_arch)-ar \
                RANLIB=$(target_arch)-ranlib
   cppunit_host := $(configure_host)
endif

# unfortunately cppunit does not recognize aarch64-linux-gnu as valid
ifeq ($(cppunit_host),--host=aarch64-linux-gnu)
   cppunit_host := --host=aarch64-unknown-linux-gnu $(make_host)
endif

SQLITE_PATH     = ../source/sqlite
OPENSSL_PATH    = ../source/openssl
MONGOOSE_PATH   = ../source/mongoose
TICPP_PATH      = ../source/ticpp

SOURCE_PATH     = $(shell echo `pwd`/../source )
BUILD_PATH      = $(shell echo `pwd` )

CPPUNIT_VER             = 1.13.2
CPPUNIT_VER_PATH        = $(shell echo `pwd`/../source/cppunit-${CPPUNIT_VER} )
CPPUNIT_INSTALL_PATH    = $(shell echo `pwd`/../source/cppunit_install)

SQLITE_VER              = 3081101
SQLITE_VER_PATH         = $(shell echo `pwd`/../source/sqlite-autoconf-${SQLITE_VER} )

OPENSSL_VER             = 2.1.6
OPENSSL_VER_PATH        = $(shell echo `pwd`/../source/libressl-${OPENSSL_VER} )

PREMAKE_VER             = 4.3
PREMAKE_VER_PATH        = $(shell echo `pwd`/../source/premake-${PREMAKE_VER} )

SQLITE_OBJ_IN   = $(SQLITE_PATH)/sqlite3.o
SQLITE_LIB_IN   = $(SQLITE_PATH)/libsqlite3.a
OPENSSL_LIB_IN  = $(OPENSSL_PATH)/ssl/.libs/libssl.a
CRYPTO_LIB_IN   = $(OPENSSL_PATH)/crypto/.libs/libcrypto.a
CPPUNIT_LIB_IN  = $(CPPUNIT_INSTALL_PATH)/usr/local/lib/libcppunit.a
MONGOOSE_LIB_IN = $(MONGOOSE_PATH)/libmongoose.a
TICPP_LIB_IN    = $(TICPP_PATH)/lib/libticpp.a

SQLITE_LIB_OUT   = ./libsqlite3.a
OPENSSL_LIB_OUT  = ./libssl.a
CRYPTO_LIB_OUT   = ./libcrypto.a
CPPUNIT_LIB_OUT  = ./libcppunit.a
MONGOOSE_LIB_OUT = ./libmongoose.a
TICPP_LIB_OUT    = ./libticpp.a

PREMAKE         := $(shell echo ${PREMAKE_VER_PATH}/bin/release/premake4)

BUILDTYPE        = $(shell `pwd`/get-build-type.sh)

FINDBUGS_HOME    = $(shell echo `pwd`/../source/findbugs)

# note: cppunit not included in "all" by default, because we prefer system lib by default
all: mongoose ticpp

sqlite:
	# we delete the entire folder for cleanup here here to be sure to remove
	# old object files indpendent of whether a previous ./configure failed.

	# note: --disable-dynamic-extensions is used to avoid having to link
	# everything with "-ldl" and we're not using dynamic extensions anyways.
	if [ ! -f $(SQLITE_LIB_IN) ]; then                            \
		rm -fr $(SQLITE_VER_PATH)                          && \
		cd $(SOURCE_PATH)                                  && \
		tar xf sqlite-autoconf-$(SQLITE_VER).tar.gz        && \
		cd $(SQLITE_PATH)                                  && \
	        ./configure --disable-dynamic-extensions $(configure_host) && \
		$(MAKE) CFLAGS+="-DSQLITE_ENABLE_UNLOCK_NOTIFY"     && \
		cd $(BUILD_PATH)                                   && \
		ar -r $(SQLITE_LIB_IN) $(SQLITE_OBJ_IN);              \
	fi

	# symlink lib
	ln -sf $(SQLITE_LIB_IN) $(SQLITE_LIB_OUT)

openssl:
	# we delete the old folders first here, to be sure to remove old object files,
	# as a clean before "configure" may have failed.
	if [ ! -f $(OPENSSL_LIB_IN) ] || [ ! -f $(CRYPTO_LIB_IN) ]; then \
		rm -fr $(OPENSSL_VER_PATH)                &&     \
		cd $(SOURCE_PATH)                         &&     \
		tar xf libressl-$(OPENSSL_VER).tar.gz     &&     \
		cd $(OPENSSL_VER_PATH)                    &&     \
		export $(make_host)                       &&     \
		./configure $(configure_host)             &&     \
		$(MAKE);                                         \
	fi

	# symlink libs
	ln -sf $(OPENSSL_LIB_IN) $(OPENSSL_LIB_OUT)
	ln -sf $(CRYPTO_LIB_IN) $(CRYPTO_LIB_OUT)

cppunit:
	# note: 'configure' complains about wrong arguments to 'dirname',
	#       but still everythings works.
	if [ ! -f $(CPPUNIT_LIB_IN) ]; then                      \
		rm -fr $(CPPUNIT_INSTALL_PATH)            &&     \
		rm -fr $(CPPUNIT_VER_PATH)                &&     \
		cd $(SOURCE_PATH)                         &&     \
		tar xf cppunit-$(CPPUNIT_VER).tar.gz      &&     \
		mkdir $(CPPUNIT_INSTALL_PATH)             &&     \
		cd $(CPPUNIT_VER_PATH)                    &&     \
		./configure $(BUILDTYPE) $(cppunit_host)  &&     \
		$(MAKE) DESTDIR=$(CPPUNIT_INSTALL_PATH) install; \
	fi
	# symlink lib
	ln -sf $(CPPUNIT_LIB_IN) $(CPPUNIT_LIB_OUT)

mongoose:
	# we run make clean first here, to be sure to remove old object files
	if [ ! -f $(MONGOOSE_LIB_IN) ]; then \
		cd $(MONGOOSE_PATH); \
		$(MAKE) clean && \
		$(MAKE) $(make_host); \
	fi
	# symlink lib
	ln -sf $(MONGOOSE_LIB_IN) $(MONGOOSE_LIB_OUT)

premake:
	if [ ! -d $(PREMAKE_VER_PATH) ]; then                    \
		cd $(SOURCE_PATH)                          &&    \
		unzip premake-${PREMAKE_VER}-src.zip       &&    \
		cd $(BUILD_PATH);                                \
	fi

	# work around a missing dependency in premake makefiles that can cause the build to fail
	mkdir -p $(PREMAKE_VER_PATH)/build/gmake.unix/obj/Release \
		$(PREMAKE_VER_PATH)/bin/release
	$(MAKE) -C $(PREMAKE_VER_PATH)/build/gmake.unix

ticpp: premake
	# we run make clean first here, to be sure to remove old object files
	# like premake, ticpp makefiles don't declare some vital dependencies
	if [ ! -f $(TICPP_LIB_IN) ]; then                     \
		cd $(TICPP_PATH)                           && \
		${PREMAKE} --unicode gmake                 && \
		$(MAKE) clean config=release               && \
		mkdir -p obj/Release                       && \
		$(MAKE) $(make_host) config=release        && \
		cd $(BUILD_PATH);                             \
	fi
	# symlink lib
	ln -sf $(TICPP_LIB_IN) $(TICPP_LIB_OUT)

clean: clean_sqlite clean_openssl clean_cppunit clean_mongoose clean_premake clean_ticpp

clean_sqlite:
	rm -fr $(SQLITE_VER_PATH)
	rm -f $(SQLITE_LIB_IN) $(SQLITE_LIB_OUT)

clean_openssl:
	rm -fr $(OPENSSL_VER_PATH)
	rm -f $(OPENSSL_LIB_OUT) $(CRYPTO_LIB_OUT)

clean_cppunit:
	rm -fr $(CPPUNIT_VER_PATH) $(CPPUNIT_INSTALL_PATH)
	rm -f $(CPPUNIT_LIB_OUT)

clean_mongoose:
	cd $(MONGOOSE_PATH); $(MAKE) clean || true
	# absolutely make sure we delete previous libs and also delete the copied lib
	rm -f $(MONGOOSE_LIB_IN) $(MONGOOSE_LIB_OUT)

clean_premake:
	rm -fr $(PREMAKE_VER_PATH)

clean_ticpp:
	cd $(TICPP_PATH); $(MAKE) clean config=release || true
	# absolutely make sure we delete previous libs and also delete the copied lib
	rm -f $(TICPP_LIB_IN) $(TICPP_LIB_OUT)
