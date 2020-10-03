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
from __future__ import division  # __future__ must be at the file beginning

import os
import argparse
import threading 
import struct
import time
import traceback
from scapy.all import *
from Queue import Queue

ICMP_ECHOREPLY = 0
ENCRYPTION_KEY = 0x1d8a2fe91cb4dd2e


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
        self._last_received_packet = None
        self._stop_event = threading.Event()

    def run(self):
        print "[+] starting to sniff ICMP packets..."
        sniff(lfilter=self.icmp_filter, prn=self.handle_new_packet, stop_filter=self.should_stop)

    def stop(self):
        self._stop_event.set()

    def handle_new_packet(self, packet):
        tmp_last_packet = self._last_received_packet
        self._last_received_packet = packet
        if packet == tmp_last_packet:
            # some implementations of loopback devices causes scapy to sniff the same packet twice.
            # in order to prevent it, we save a copy of the previous packet and ignore the new one
            # if both are the same.
            return
        self.received_chunks_queue.put(FileChunk(data=packet[Padding].load))

    @staticmethod
    def icmp_filter(pkt):
        return pkt.haslayer(ICMP) and ICMP_ECHOREPLY == pkt[ICMP].type and pkt.haslayer(Padding)

    def should_stop(self, pkt):
        return self._stop_event.isSet()


class FileSaver(threading.Thread):
    """
    """
    def __init__(self, received_chunks_queue, is_compressed=False, is_encrypted=False):
        super(FileSaver, self).__init__()
        self.received_chunks_queue = received_chunks_queue
        self.is_compressed = is_compressed
        self.is_encrypted = is_encrypted
        self._stop_event = threading.Event()

    def run(self):
        while not self._stop_event.isSet():
            print "[+] waiting for files..."
            try:
                file_chunk = self._get_next_chunk()
                while not self._is_first_chunk(file_chunk):
                    file_chunk = self._get_next_chunk()

                file_size = self._get_file_size_from_chunk(file_chunk)
                file_chunk.data = file_chunk.data[Signature.MAGIC_LEN + Signature.FILE_SIZE_LEN:]

                print "[+] found pending file with length {} bytes".format(file_size)

                file_data = ''
                while True:
                    size_to_read = min(len(file_chunk.data), file_size - len(file_data))
                    file_data += file_chunk.data[:size_to_read]

                    print "[+] downloading: {}% ({}/{} bytes)".format(int((len(file_data) / file_size) * 100.0), len(file_data), file_size)

                    if len(file_data) == file_size:
                        break                    

                    file_chunk = self._get_next_chunk()
                
                if self.is_compressed:
                    file_data = self._extract_data(compressed_data=file_data)
                if self.is_encrypted:
                    file_data = self._decrypt_data(encrypted_data=file_data, key=ENCRYPTION_KEY)

                with open("received_file", "wb") as f:
                    f.write(file_data)

                print "[+] saved {} bytes to {}".format(len(file_data), os.path.abspath("received_file"))
            except Exception as e:
                print "[+] ERROR: received an exception while trying to fetch file."
                print "[+] {}.".format(traceback.format_exc())
                print "[+] continuing..."

    def stop(self):
        self._stop_event.set()

    def _decrypt_data(self, encrypted_data, key):
        encryption_key_length = (len(hex(key)) - len('0x')) // 2

        decrypted_data = ''
        for i in xrange(len(encrypted_data)):
            decrypted_data += chr(ord(encrypted_data[i]) ^ (key >> (8 * (i % encryption_key_length)) & 0xFF))

        return decrypted_data

    def _extract_data(self, compressed_data):
        return compressed_data

    def _get_next_chunk(self):
        return self.received_chunks_queue.get()

    @staticmethod
    def _is_first_chunk(chunk):
        return chunk.data[Signature.MAGIC_OFFSET:Signature.MAGIC_LEN] == Signature.MAGIC_DATA

    @staticmethod
    def _get_file_size_from_chunk(chunk):
        file_size_as_stringhex = chunk.data[Signature.FILE_SIZE_OFFSET:
                                            Signature.FILE_SIZE_OFFSET + Signature.FILE_SIZE_LEN]
        return struct.unpack("l", file_size_as_stringhex)[0] 


def main():
    received_chunks_queue = Queue()

    sniffer = ICMPSniffer(received_chunks_queue=received_chunks_queue)
    file_saver = FileSaver(received_chunks_queue=received_chunks_queue, is_encrypted=True)

    sniffer.daemon = True 
    file_saver.daemon = True
    
    sniffer.start()
    file_saver.start()

    start_time = time.time()
    try:
        while sniffer.is_alive() and file_saver.is_alive():
            sniffer.join(1)
            file_saver.join(1)
    except KeyboardInterrupt:
        print
        print "[+] recieved Ctrl-C, aborting..."

    sniffer.stop()
    file_saver.stop()

    end_time = time.time()

    print "---------------------------------------"
    print "[+] finished in {0} seconds.".format(end_time - start_time)

    return 0

if __name__ == "__main__":
    main()
