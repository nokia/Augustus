# Config
SHELL = /bin/sh

# RTE_SDK points to the directory where DPDK is built
ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

DOC_DIR = doc
BUILD_DIR = build

# Binary name
APP = dpdk-content-router

# This is needed if we build an extlib
LIB = $(APP)

# Folders containing source files (relative path)
SRC_LIB_DIR = src/lib
SRC_MAIN_DIR = src/main
SRC_CONFIG_DIR = src/config
SRC_UTIL_DIR = src/util
#
## all source are stored in SRCS-y
SRCS-y := $(SRC_MAIN_DIR)/main.c
SRCS-y += $(SRC_MAIN_DIR)/data_plane.c $(SRC_MAIN_DIR)/init.c $(SRC_MAIN_DIR)/control_plane.c

SRCS-y +=  $(SRC_LIB_DIR)/fib/fib.c $(SRC_LIB_DIR)/fib/fib_hash_table.c
SRCS-y += $(SRC_LIB_DIR)/pit/pit.c
SRCS-y += $(SRC_LIB_DIR)/cs/cs.c
SRCS-y += $(SRC_LIB_DIR)/util.c
SRCS-y += $(SRC_LIB_DIR)/packet.c

# Here for the -I option (which locates headers) I need absolute path
CFLAGS += -O3 -I$(SRCDIR)/$(SRC_LIB_DIR) -I$(SRCDIR)/$(SRC_MAIN_DIR) -I$(SRCDIR)/$(SRC_CONFIG_DIR)
#CFLAGS += $(WERROR_FLAGS)

include $(RTE_SDK)/mk/rte.extapp.mk
# Note: it's important that additional targets are declared after including
# rte.extapp.mk, otherwise they won't work
all: $(APP) util

util: 
	cc -I$(SRCDIR)/$(SRC_LIB_DIR) -I$(SRCDIR)/$(SRC_CONFIG_DIR) $(SRC_UTIL_DIR)/fib_control.c -o $(BUILD_DIR)/fib-ctrl

# Build documentation
doc: Doxyfile
	doxygen Doxyfile

# Clean documentation
docclean:
	rm -rf $(DOC_DIR)

# Clean everithing 
deepclean:
	rm -rf $(DOC_DIR)
	rm -rf $(BUILD_DIR)

