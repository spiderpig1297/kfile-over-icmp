MODULENAME ?= kfile_over_icmp

# KERNELDIR ?= ~/workspace/buildroot/output/build/linux-4.19.98
KERNELDIR ?= /lib/modules/4.10.0-38-generic/build
PWD       := $(shell pwd)

RM = rm -rf

.PHONY: all debug release install remove clean

all: release

release:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

debug: 
	$(MAKE) -C $(KERNELDIR) M=$(PWD) DEBUG=1
	
install:
	sudo insmod $(MODULENAME).ko
remove:
	sudo rmmod $(MODULENAME).ko
clean:
	@ $(RM) *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions debug modules.order Module.symvers