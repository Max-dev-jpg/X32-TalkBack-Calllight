#!/usr/bin/env python3
"""
osc_monitor.py – Subscribe to X32 updates via /xremote and print all incoming OSC messages.

Useful for discovering OSC paths and verifying meter/fader data.

Usage:
    python osc_monitor.py --ip 192.168.0.100

Cross-platform: works on macOS, Windows, Linux.
"""

import argparse
import socket
import struct
import time
import threading


def osc_pad(data):
    r = len(data) % 4
    return data + (b'\x00' * (4 - r) if r else b'')

def osc_string(s):
    return osc_pad(s.encode('ascii') + b'\x00')

def build_query(addr):
    return osc_string(addr) + osc_string(',')

def read_string(data, offset):
    end = data.index(b'\x00', offset)
    s = data[offset:end].decode('ascii', errors='replace')
    offset = end + 1
    if offset % 4:
        offset += 4 - (offset % 4)
    return s, offset

def parse_all(data):
    try:
        addr, offset = read_string(data, 0)
        typetag, offset = read_string(data, offset)
        args = []
        for t in typetag[1:]:
            if t == 'f' and offset + 4 <= len(data):
                args.append(('f', struct.unpack('>f', data[offset:offset+4])[0]))
                offset += 4
            elif t == 'i' and offset + 4 <= len(data):
                args.append(('i', struct.unpack('>i', data[offset:offset+4])[0]))
                offset += 4
            elif t == 's':
                s, offset = read_string(data, offset)
                args.append(('s', s))
            elif t == 'b' and offset + 4 <= len(data):
                blen = struct.unpack('>I', data[offset:offset+4])[0]
                offset += 4
                args.append(('b', f'[blob {blen} bytes]'))
                offset += blen + ((4 - blen % 4) % 4)
            else:
                args.append((t, '?'))
        return addr, typetag, args
    except Exception as e:
        return None, None, [('err', str(e))]


def xremote_sender(sock, ip, port):
    """Thread: send /xremote every 8 seconds to keep subscription alive."""
    msg = build_query('/xremote')
    while True:
        try:
            sock.sendto(msg, (ip, port))
        except Exception:
            pass
        time.sleep(8)


def main():
    parser = argparse.ArgumentParser(description='X32 OSC monitor')
    parser.add_argument('--ip',     default='192.168.0.100')
    parser.add_argument('--port',   type=int, default=10023)
    parser.add_argument('--rxport', type=int, default=10024)
    parser.add_argument('--filter', default='', help='Only show paths containing this string')
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', args.rxport))
    sock.settimeout(1.0)

    t = threading.Thread(target=xremote_sender, args=(sock, args.ip, args.port), daemon=True)
    t.start()

    print(f'Monitoring {args.ip}:{args.port}  (listening on {args.rxport})')
    if args.filter:
        print(f'Filter: "{args.filter}"')
    print('Press Ctrl+C to stop.\n')

    try:
        while True:
            try:
                data, _ = sock.recvfrom(4096)
                addr, tag, arg_list = parse_all(data)
                if addr is None:
                    continue
                if args.filter and args.filter not in addr:
                    continue
                arg_str = '  '.join(f'{t}={v}' for t, v in arg_list)
                print(f'{addr:<40}  {tag:<12}  {arg_str}')
            except socket.timeout:
                pass
    except KeyboardInterrupt:
        print('\nStopped.')
    finally:
        sock.close()


if __name__ == '__main__':
    main()
