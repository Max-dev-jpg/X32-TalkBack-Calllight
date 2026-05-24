#!/usr/bin/env python3
"""
osc_test.py – Send OSC queries to a Behringer X32 / Midas M32 and print responses.

Usage:
    python osc_test.py --ip 192.168.0.100 --path /dca/1/fader
    python osc_test.py --ip 192.168.0.100 --path /ch/01/mix/fader --interval 0.2

Requirements:
    pip install python-osc

Cross-platform: works on macOS, Windows, Linux.
"""

import argparse
import socket
import struct
import time


# ── Minimal OSC encoder (no dependency) ──────────────────────────────────────

def osc_pad(data: bytes) -> bytes:
    """Pad bytes to 4-byte boundary with null bytes."""
    r = len(data) % 4
    if r:
        data += b'\x00' * (4 - r)
    return data

def osc_string(s: str) -> bytes:
    """Encode an OSC string (null-terminated, padded to 4 bytes)."""
    return osc_pad(s.encode('ascii') + b'\x00')

def build_query(address: str) -> bytes:
    """Build an OSC query message (address with empty type tag)."""
    return osc_string(address) + osc_string(',')

def build_string_msg(address: str, arg: str) -> bytes:
    """Build an OSC message with one string argument."""
    return osc_string(address) + osc_string(',s') + osc_string(arg)


# ── Minimal OSC decoder ───────────────────────────────────────────────────────

def read_osc_string(data: bytes, offset: int):
    end = data.index(b'\x00', offset)
    s = data[offset:end].decode('ascii', errors='replace')
    # Advance past null + padding to next 4-byte boundary
    offset = end + 1
    if offset % 4:
        offset += 4 - (offset % 4)
    return s, offset

def parse_response(data: bytes):
    try:
        addr, offset = read_osc_string(data, 0)
        typetag, offset = read_osc_string(data, offset)

        result = {'address': addr, 'type': typetag, 'value': None}

        if len(typetag) >= 2:
            t = typetag[1]
            if t == 'f' and offset + 4 <= len(data):
                result['value'] = struct.unpack('>f', data[offset:offset+4])[0]
                result['type']  = 'float'
            elif t == 'i' and offset + 4 <= len(data):
                result['value'] = struct.unpack('>i', data[offset:offset+4])[0]
                result['type']  = 'int'
            elif t == 's':
                result['value'], _ = read_osc_string(data, offset)
                result['type']     = 'string'
        return result
    except Exception as e:
        return {'error': str(e), 'raw': data.hex()}


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description='OSC query tool for X32/M32')
    parser.add_argument('--ip',       default='192.168.0.100', help='Mixer IP address')
    parser.add_argument('--port',     type=int, default=10023,  help='Mixer OSC port')
    parser.add_argument('--rxport',   type=int, default=10024,  help='Local listen port')
    parser.add_argument('--path',     default='/dca/1/fader',   help='OSC path to query')
    parser.add_argument('--interval', type=float, default=0.5,  help='Poll interval (s)')
    parser.add_argument('--count',    type=int,   default=0,     help='Polls (0=infinite)')
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', args.rxport))
    sock.settimeout(0.5)

    print(f'Querying  {args.ip}:{args.port}  path={args.path}')
    print(f'Listening on port {args.rxport}')
    print('Press Ctrl+C to stop.\n')

    query = build_query(args.path)
    xremote = build_query('/xremote')

    count = 0
    last_xremote = 0

    try:
        while args.count == 0 or count < args.count:
            now = time.time()

            # Renew /xremote every 8 seconds
            if now - last_xremote >= 8:
                sock.sendto(xremote, (args.ip, args.port))
                last_xremote = now

            # Send query
            sock.sendto(query, (args.ip, args.port))
            count += 1

            # Wait for response
            try:
                data, addr = sock.recvfrom(1024)
                parsed = parse_response(data)
                if 'error' in parsed:
                    print(f'[{count:4d}] Parse error: {parsed}')
                else:
                    val = parsed['value']
                    if isinstance(val, float):
                        print(f'[{count:4d}] {parsed["address"]}  = {val:.4f}  ({parsed["type"]})')
                    else:
                        print(f'[{count:4d}] {parsed["address"]}  = {val}  ({parsed["type"]})')
            except socket.timeout:
                print(f'[{count:4d}] Timeout – no response from {args.ip}')

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print('\nStopped.')
    finally:
        sock.close()


if __name__ == '__main__':
    main()
