obj-m += khidden-file-sender.o
khidden-file-sender-y := source.o checksum.o core.o netfilter.o
ccflags-y := -O0

KERNELDIR ?= /lib/modules/4.10.0-38-generic/build
PWD       := $(shell pwd)

debug:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
release:
	$(MAKE) -C $(rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions debug) M=$(PWD) modules
install:
	sudo insmod khidden-file-sender.ko
remove:
	sudo rmmod khidden-file-sender.ko
clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions debug