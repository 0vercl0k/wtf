# Axel '0vercl0k' Souchet - February 12 2022
import sys
import json
import socket
import struct

def main():
    if len(sys.argv) != 3:
        print('./tlv_client <ip> <testcase.json>')
        return
    
    _, ip, testcase_path = sys.argv
    root = json.load(open(testcase_path))
    s = socket.create_connection((ip, 4444))
    for idx, packet in enumerate(root['Packets']):
        serialized = struct.pack('<IHH', packet['Command'], packet['Id'], packet['BodySize'])
        serialized += bytes(packet['Body'])
        print(f'Sending {len(serialized)} bytes for packet {idx}..')
        s.send(struct.pack('<I', len(serialized)))
        s.send(serialized)

    print('Done')

if __name__ == '__main__':
    main()
