SHELL := /bin/bash

SOURCE_PATH = $(shell echo `pwd`/../source )

PCOPY_VER            = 0.96
PCOPY_TAR            = $(SOURCE_PATH)/pcopy-$(PCOPY_VER).tar.gz
PCOPY_PATH           = $(SOURCE_PATH)/pcopy-$(PCOPY_VER)
PCOPY_BIN_PATH       = $(SOURCE_PATH)/pcopy-$(PCOPY_VER)/bin
PCOPY_SCRIPTS_PATH   = $(SOURCE_PATH)/pcopy-$(PCOPY_VER)/scripts
PCOPY_DOC            = $(SOURCE_PATH)/pcopy-$(PCOPY_VER)/README.txt
PCOPY_OUT_PATH       = ./pcopy/
GPI2_VER             = 1.2.0
GPI2_TAR             = $(SOURCE_PATH)/GPI2-$(GPI2_VER).tar.gz
GPI2_PATH            = $(SOURCE_PATH)/GPI2-$(GPI2_VER)
GPI2_OUT_PATH        = $(SOURCE_PATH)/GPI2-$(GPI2_VER)/out
MCTP3_TAR            = $(SOURCE_PATH)/mctp3.tar.gz
MCTP3_PATH           = $(SOURCE_PATH)/mctp3

.PHONY: all clean rpm deb 

all: pcopy gpi mctp

gpi: $(GPI2_TAR)
	tar -C $(SOURCE_PATH) -xzf $(GPI2_TAR)
	cd $(GPI2_PATH) && ./install.sh -p $(GPI2_OUT_PATH) --with-ethernet

mctp: $(MCTP3_TAR)
	tar -C $(SOURCE_PATH) -xzf $(MCTP3_TAR)        

pcopy: $(PCOPY_TAR) mctp gpi
	tar -C $(SOURCE_PATH) -xzf $(PCOPY_TAR) 
	make -C $(PCOPY_PATH) GPI2_INCLUDE_PATH=$(GPI2_OUT_PATH)/include \
	MCTP3_INCLUDE_PATH=$(MCTP3_PATH)/include GPI2_LIB_PATH=$(GPI2_OUT_PATH)/lib64 \
	MCTP3_LIB_PATH=$(MCTP3_PATH)/lib
	mkdir -p $(PCOPY_OUT_PATH)
	cp $(PCOPY_BIN_PATH)/* $(PCOPY_OUT_PATH)/ 
	cp $(PCOPY_SCRIPTS_PATH)/* $(PCOPY_OUT_PATH)/
	cp $(PCOPY_DOC) $(PCOPY_OUT_PATH)/

clean:
	rm -Rf $(GPI2_PATH)
	rm -Rf $(PCOPY_PATH)
	rm -Rf $(MCTP3_PATH)
	rm -Rf $(PCOPY_OUT_PATH)
