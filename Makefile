MODULENAME := kfile_over_icmp

obj-m += $(MODULENAME).o

# chrdev
$(MODULENAME)-y += src/chrdev/chrdev.o

# fs
$(MODULENAME)-y += src/fs/payload_generator.o

# net
$(MODULENAME)-y += src/net/checksum.o src/net/netfilter.o

# core
$(MODULENAME)-y += src/init.o src/core.o

ccflags-y := -O0 -Wno-declaration-after-statement -Wno-discarded-qualifiers

# KERNELDIR ?= ~/workspace/buildroot/output/build/linux-4.19.98
KERNELDIR ?= /lib/modules/4.10.0-38-generic/build
PWD       := $(shell pwd)

debug:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
release:
	$(MAKE) -C $(rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions debug) M=$(PWD) modules
install:
	sudo dmesg -C	
	sudo insmod khidden_file_sender.ko
remove:
	sudo rmmod khidden_file_sender.ko
clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions debug