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
___

Once installed, the module registers a character device from which it gets the paths of the files to send.
Run `dmesg` to retrieve the major number of the device, and then create a node to it using `mknod`:

```sh
$ dmesg | tail

[15132.935862] kfile-over-icmp: LKM loaded
[15132.935864] kfile-over-icmp: registered netfilter hook
[15132.935867] kfile-over-icmp: succesfully registered character device. major: 243

$ mknod /dev/readfile c <MAJOR_GOES_HERE> 0
```

In order to send a file over ICMP, write its (absolute!) path to our newly-created device:

```sh
$ echo "/file/to/send/ > /dev/readfile
```

__From now on, once an ICMP-request (ping) packet will reach the machine, the module will inject the file's data on the ICMP-reply packet.__

For more information, see [How It Works](#how-it-works).

___ 

To uninstall the module, run:

```sh
$ make remove
```

## __How It Works__
___ 

- [Why an LKM?](#why-an-lkm)
- [Mangling outgoing packets](#mangling-outgoing-packets)
- [Injecting data to ICMP packets](#injecting-data-to-icmp-packets)
- [Reading user-space files](#reading-user-space-files)
___ 
### Why an LKM?

You probably ask yourselves why to implement this complicated logic as a kernel module. It is harder to implement and it can certainly be implemented in user-mode by using _netfilter_'s user-mode functionality.

Well, there are few reasons why I chose to implement it as an LKM:
- It is by far more fun and challenging. Implementing this module requires complicated mechanisms such as injecting data to ICMP packets and reading and splitting to chunks files from the user-space.
- It is by far more stealth then a user-mode program. There is no network-related system-command (`iptables`, `netstat`, etc) that will ever be able to know that a driver listens on ICMP and injects data on the replies.
- Since we are dealing with files from the kernel, each file's metadata is left untouched. It means that reading the file doesn't change it modification time (mtime) nor its access time (atime) or creation time (ctime).

### Mangling outgoing packets

The module uses _netfilter_ to intercept every outgoing packet in the system.

From netfilter's documentation:

    netfilter is a set of hooks inside the Linux kernel that allows kernel modules to register callback functions with the network stack. A registered callback function is then called back for every packet that traverses the respective hook within the network stack.

netfilter offers a number of different places where a user can place hooks:

    --->[1]--->[ROUTE]--->[3]--->[4]--->
                 |            ^
                 |            |
                 |         [ROUTE]
                 v            |
                [2]          [5]
                 |            ^
                 |            |
                 v            |

    [1] NF_IP_PER_ROUNTING — triggered by any incoming traffic very soon after entering the network 
                             stack. This hook is processed before any routing decisions have been made 
                             regarding where to send the packet.
    [2] NF_IP_LOCAL_IN —     triggered after an incoming packet has been routed if the packet is destined 
                             for the local system.
    [3] NF_IP_FORWARD —      triggered after an incoming packet has been routed if the packet is to be 
                             forwarded to another host.
    [4] NF_IP_POST_ROUTING — triggered by any outgoing or forwarded traffic after routing has taken place 
                             and just before "hitting the wire".
    [5] NF_IP_LOCAL_OUT —    triggered by any locally created outbound traffic as soon it hits the network 
                             stack.

The module places an hook on `NF_IP_POST_ROUTING` in order to intercept every outgoing packets, means that
the callback will be called for every packet sent from the machine, with the packet as its argument.

Then the module checks whether the packet's protocol is ICMP and its type is ICMP-reply. Every packet that
doesn't meet the requirements is ignored and left untouched. Every packet that meets the requirements will
later be injected with the file data.

### Injecting data to ICMP packets

This is the time to admit that this title is a bit misleading.

[ICMP](https://en.wikipedia.org/wiki/Internet_Control_Message_Protocol) packets are very narrowed in places
where we can inject our data. To refresh your memory, here is the ICMP header layout taken from its [RFC](https://tools.ietf.org/html/rfc792):

    Echo or Echo Reply Message

     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |     Type      |     Code      |          Checksum             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |           Identifier          |        Sequence Number        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |     Data ...
    +-+-+-+-+-
    
As we can see, the ICMP header is very small. If its not enough, its data must be the same between the reply and request packets, otherwise each side won't treat the packet as valid (hence won't print the famous `64 from 192.168.1.1 ...` output).

Since injecting our data on one of the above fields is not possible without causing anomally in both sides,
we have to find another way. 

What if we will inject our data __after__ the end of the ICMP layer?

A quick research shows that this is possible.

_INSERT IMAGE HERE_

Whenever the module identifies an outgoing ICMP-reply packet, it shoves the desired payload after the ICMP layer of the packet. The method of injecting payload as a padding layer has several advantages:
- __The data of both the ICMP requests and ICMP replies stays the same__, maintainig the RFC requirements and avoiding possible anomalies.
- Since we are not updating the size of the packet in the IP header (`tot_len`), __one can almost certainly
assume that no one will ever know that the payloads exists__. Most of the services or servers reads the _*tot_len*_ field to know how much data they need to read. It is important to note that advanced systems such as IDS/IPS or any other DPI (Deep Packet Inspection) systems will probably recognize that anomaly, but we can assume that we will not encounter them very often.
- __The implementation is simpler__, needing only to add data and not to modify existing headers (except from the checksum).



### Reading user-space files