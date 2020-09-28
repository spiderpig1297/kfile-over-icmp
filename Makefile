MODULENAME := khidden_file_sender

obj-m += $(MODULENAME).o

# chrdev
$(MODULENAME)-y += chrdev/chrdev.o

# fs
$(MODULENAME)-y += fs/payload_generator.o

# net
$(MODULENAME)-y += net/checksum.o net/netfilter.o

# core
$(MODULENAME)-y += source.o core.o

ccflags-y := -O0 -Wno-declaration-after-statement

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