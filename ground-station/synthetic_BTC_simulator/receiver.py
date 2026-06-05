import asyncio
import struct
import argparse
import sys
import os
from collections import defaultdict

SYNC_0 = 0xB0
SYNC_1 = 0x17
VERSION = 0x01
HEADER_SIZE = 12

stats = {
    'total_packets': 0,
    'crc_errors': 0,
    'types_last_sec': defaultdict(int)
}

def calculate_crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8) & 0xFFFF
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

async def dashboard_updater():
    sys.stdout.write('\033[?25l') #hided cursor, fixed einiges
    try:
        while True:
            await asyncio.sleep(1.0)
            
            # Clear screen
            os.system('clear' if os.name == 'posix' else 'cls')
            
            print("=== Rexus Telemetry Live Stats (Updated every 1s) ===")
            print(f"Total Packets: {stats['total_packets']} | Total CRC Errors: {stats['crc_errors']}")
            
            rate = sum(stats['types_last_sec'].values())
            print(f"Current Rate : {rate} pkts/sec\n")
            print("Arrival Breakdown (Packets in last second):")
            
            for ptype, count in sorted(stats['types_last_sec'].items()):
                bar_len = min(count, 50)
                bar = '▇' * bar_len
                print(f"Type 0x{ptype:02X}: {bar} {count}")
                
            print("=====================================================")
            
            # Reset counts für nächste Sekunde
            stats['types_last_sec'].clear()
    except asyncio.CancelledError:
        pass
    finally:
        sys.stdout.write('\033[?25h') #pls no break

def process_telemetry_buffer(buffer: bytearray, sync_state: int, dashboard_mode: bool) -> int:
    while True:
        if sync_state == 0:
            idx = buffer.find(SYNC_0)
            if idx == -1:
                buffer.clear()
                break
            
            if idx > 0:
                del buffer[:idx]
                
            if len(buffer) >= 2:
                if buffer[1] == SYNC_1:
                    sync_state = 1
                else:
                    buffer.pop(0)
                    continue
            else:
                break
        
        if sync_state == 1:
            if len(buffer) < HEADER_SIZE:
                break
                
            (s0, s1, version, ptype, seq, length, tick, tstamp) = struct.unpack(
                '<BBBBBBHI', buffer[:HEADER_SIZE]
            )
            
            if version != VERSION:
                if not dashboard_mode:
                    print(f"[!] Invalid version {version}. Resynchronizing...")
                buffer.pop(0)
                sync_state = 0
                continue
                
            packet_size = HEADER_SIZE + length + 2
            
            if len(buffer) < packet_size:
                break
                
            packet = buffer[:packet_size]
            del buffer[:packet_size]
            sync_state = 0
            
            crc_data = packet[2:HEADER_SIZE+length]
            expected_crc = calculate_crc16(crc_data)
            
            received_crc = struct.unpack('>H', packet[HEADER_SIZE+length:])[0]
            
            stats['total_packets'] += 1
            
            if expected_crc != received_crc:
                stats['crc_errors'] += 1
                if not dashboard_mode:
                    print(f"[!] CRC Error! Type: 0x{ptype:02X}, Seq: {seq}, Expected: 0x{expected_crc:04X}, Got: 0x{received_crc:04X}")
            else:
                if dashboard_mode:
                    stats['types_last_sec'][ptype] += 1
                else:
                    print(f"[+] Valid Packet - Type: 0x{ptype:02X}, Seq: {seq:03d}, Length: {length:02d}, Tick: {tick:05d}, Total: {stats['total_packets']}")
    return sync_state

async def telemetry_receiver(host='127.0.0.1', port=5000, dashboard_mode=False):
    if not dashboard_mode:
        print(f"[*] Connecting to telemetry server at {host}:{port}...")
    try:
        reader, writer = await asyncio.open_connection(host, port)
        if not dashboard_mode:
            print("[*] Connected successfully.")
    except Exception as e:
        if not dashboard_mode:
            print(f"[!] Failed to connect: {e}")
        return

    sync_state = 0
    buffer = bytearray()

    while True:
        try:
            chunk = await reader.read(4096)
            if not chunk:
                if not dashboard_mode:
                    print("[-] Server disconnected.")
                break
                
            buffer.extend(chunk)
            sync_state = process_telemetry_buffer(buffer, sync_state, dashboard_mode)
                        
        except Exception as e:
            if not dashboard_mode:
                print(f"[!] Error reading from stream: {e}")
            break
            
    writer.close()
    await writer.wait_closed()
    if not dashboard_mode:
        print(f"\n[*] Disconnected. Received {stats['total_packets']} packets with {stats['crc_errors']} CRC errors.")

async def telemetry_receiver_pcap(pcap_file, dashboard_mode=False):
    from scapy.all import PcapReader, Raw
    if not dashboard_mode:
        print(f"[*] Analyzing PCAP file: {pcap_file}")
        
    sync_state = 0
    buffer = bytearray()
    last_ts = None
    
    try:
        with PcapReader(pcap_file) as pcap_reader:
            for pkt in pcap_reader:
                if Raw in pkt:
                    data = bytes(pkt[Raw])
                    buffer.extend(data)
                    
                    if dashboard_mode:
                        if last_ts is not None:
                            delay = float(pkt.time) - last_ts
                            if delay > 0:
                                await asyncio.sleep(delay)
                        last_ts = float(pkt.time)
                    
                    sync_state = process_telemetry_buffer(buffer, sync_state, dashboard_mode)
    except Exception as e:
        if not dashboard_mode:
            print(f"[!] Error reading PCAP: {e}")
            
    if not dashboard_mode:
        print(f"\n[*] PCAP Analysis Complete. Received {stats['total_packets']} packets with {stats['crc_errors']} CRC errors.")

async def main():
    parser = argparse.ArgumentParser(description="Telemetry Receiver")
    parser.add_argument('--dashboard', action='store_true', help="Enable live visualization dashboard")
    parser.add_argument('--pcap', type=str, help="Analyze PCAP file instead of connecting to live server")
    args = parser.parse_args()
    
    if args.pcap:
        receiver_task = asyncio.create_task(telemetry_receiver_pcap(args.pcap, dashboard_mode=args.dashboard))
    else:
        receiver_task = asyncio.create_task(telemetry_receiver(dashboard_mode=args.dashboard))
        
    dashboard_task = None
    if args.dashboard:
        dashboard_task = asyncio.create_task(dashboard_updater())
        
    await receiver_task
    if dashboard_task:
        dashboard_task.cancel()

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        if '--dashboard' not in sys.argv:
            print("\n[*] Receiver shutdown")
        sys.stdout.write('\033[?25h')
