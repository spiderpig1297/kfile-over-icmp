#!/usr/bin/python
"""
This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.
"""
import argparse
import threading 
import struct
from scapy.all import *
from Queue import Queue

ICMP_ECHOREPLY = 0


class Signature(object):
    # Magic
    MAGIC_OFFSET = 0
    MAGIC_LEN = 8
    MAGIC_DATA = '\xde\xad\xbe\xef\xde\xad\xbe\xef'
    # File size
    FILE_SIZE_OFFSET = 8
    FILE_SIZE_LEN = 8


class FileChunk(object):
    def __init__(self, data):
        self.data = data


class ICMPSniffer(threading.Thread):
    """
    """
    def __init__(self, received_chunks_queue, interface="any"):
        super(ICMPSniffer, self).__init__()
        self.interface = interface
        self.received_chunks_queue = received_chunks_queue
        self._stop_event = threading.Event()

    def run(self):
        """
        """
        print "[+] starting to sniff ICMP packets..."
        sniff(lfilter=self.icmp_filter, prn=self.handle_new_packet, stop_filter=self.should_stop)
        print "[+] finished sniffing ICMP packets."

    def stop(self):
        self._stop_event.set()

    def handle_new_packet(self, packet):
        self.received_chunks_queue.put(FileChunk(data=packet[Padding].load))

    @staticmethod
    def icmp_filter(pkt):
        return pkt.haslayer(ICMP) and ICMP_ECHOREPLY == pkt[ICMP].type and pkt.haslayer(Padding)

    def should_stop(self, pkt):
        return self._stop_event.isSet()

def signal_handler(sig, frame):
    print('You pressed Ctrl+C!')
    sys.exit(0)

def main():
    received_chunks_queue = Queue()
    sniffer = ICMPSniffer(received_chunks_queue=received_chunks_queue)
    sniffer.start()

    while True:
        # get next chunks until we gound the beginning of a file
        file_chunk = received_chunks_queue.get()
        while file_chunk.data[Signature.MAGIC_OFFSET:Signature.MAGIC_LEN] != Signature.MAGIC_DATA:
            file_chunk = received_chunks_queue.get()

        file_size_as_stringhex = file_chunk.data[Signature.FILE_SIZE_OFFSET:
                                                Signature.FILE_SIZE_OFFSET + Signature.FILE_SIZE_LEN]
        file_size = struct.unpack("l", file_size_as_stringhex)[0]

        file_chunk.data = file_chunk.data[Signature.MAGIC_LEN + Signature.FILE_SIZE_LEN:]

        print "[+] found file with length {} bytes".format(file_size)
        file_data = ''
        while len(file_data) != file_size:
            size_left_to_fill = file_size - len(file_data)
            if (len(file_chunk.data) < size_left_to_fill):
                file_data += file_chunk.data
            else:
                file_data += file_chunk.data[:size_left_to_fill]

            file_chunk = received_chunks_queue.get()

        with open("file", "wb") as f:
            f.write(file_data)
        print "[+] saved {} bytes to {}".format(len(file_data), "file")

    sniffer.stop()
    return 0

if __name__ == "__main__":
    main()