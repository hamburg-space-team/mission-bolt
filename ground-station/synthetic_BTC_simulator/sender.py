import asyncio
import struct
import time
import random
import argparse

SYNC_0 = 0xB0
SYNC_1 = 0x17
VERSION = 0x01

PAYLOAD_TYPES = {
    'BOOT': 0xFE,
    'GAP_MARKER': 0xF0,
    'BTC_ENV': 0x10,
    'BTC_STATUS': 0x11,
    'EXP1_SPECTRUM_A': 0x20,
    'EXP1_SPECTRUM_B': 0x21,
    'EXP1_ENV': 0x22,
    'EXP1_STATUS': 0x23,
    'EXP2_BER': 0x30,
    'EXP2_STATUS': 0x31,
    'EXP2_ENV': 0x32,
    'EXP3_STACK_A': 0x40,
    'EXP3_STACK_B': 0x41,
    'EXP3_ENV': 0x42,
    'EXP3_STATUS': 0x43,
}

FREQUENCIES = {
    'BTC_ENV': 25.0,
    'BTC_STATUS': 1.0,
    'EXP1_SPECTRUM_A': 7.5,
    'EXP1_SPECTRUM_B': 7.5,
    'EXP1_ENV': 25.0,
    'EXP1_STATUS': 1.0,
    'EXP2_BER': 25.0,
    'EXP2_ENV': 25.0,
    'EXP2_STATUS': 1.0,
    'EXP3_STACK_A': 50.0,
    'EXP3_STACK_B': 50.0,
    'EXP3_ENV': 25.0,
    'EXP3_STATUS': 1.0,
}

PAYLOAD_SIZES = {
    'EXP1_SPECTRUM_A': 40, # 54B - 14B header/crc
    'EXP1_SPECTRUM_B': 40,
    'EXP1_ENV': 12,        # 26B - 14B
    'EXP1_STATUS': 8,      # 22B - 14B
    'EXP2_BER': 20,        # 34B - 14B
    'EXP2_ENV': 12,        # 26B - 14B
    'EXP2_STATUS': 12,     # 26B - 14B
    'EXP3_STACK_A': 40,    # 54B - 14B
    'EXP3_STACK_B': 40,    # 54B - 14B
    'EXP3_ENV': 24,        # 38B - 14B
    'EXP3_STATUS': 20,     # 34B - 14B
}

def calculate_crc16(data: bytes) -> int:
    """Calculates CRC-16-CCITT (poly 0x1021, init 0xFFFF)."""
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8) & 0xFFFF
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

class TelemetrySender:
    def __init__(self):
        self.seq_counters = {ptype: 0 for ptype in PAYLOAD_TYPES.values()}
        self.start_time = time.time()
        self.clients = set()
        self.tick_counter = 0
        
    async def handle_client(self, reader, writer):
        addr = writer.get_extra_info('peername')
        print(f"[*] New client connected: {addr}")
        self.clients.add(writer)
        try:
            while True:
                await asyncio.sleep(3600)  
        except asyncio.CancelledError:
            pass
        except ConnectionError:
            pass
        finally:
            print(f"[*] Client disconnected: {addr}")
            if writer in self.clients:
                self.clients.remove(writer)
            writer.close()
            await writer.wait_closed()

    def build_packet(self, payload_type_name: str, payload_data: bytes) -> bytes:
        ptype = PAYLOAD_TYPES[payload_type_name]
        
        seq = self.seq_counters[ptype]
        self.seq_counters[ptype] = (seq + 1) & 0xFF
        
        length = len(payload_data)
        if length > 50:
            raise ValueError(f"Payload exceeds 50 bytes maximum (got {length})")
            
        elapsed_us = int((time.time() - self.start_time) * 1_000_000)
        
        # Header layout (12 bytes):
        # 0: sync[0] (0xB0)
        # 1: sync[1] (0x17)
        # 2: version (0x01)
        # 3: type
        # 4: sequence
        # 5: length
        # 6-7: tick (uint16_t, little endian)
        # 8-11: timestamp_us (uint32_t, little endian)
        
        header = struct.pack('<BBBBBBHI',
            SYNC_0, SYNC_1, VERSION, ptype, seq, length, self.tick_counter & 0xFFFF, elapsed_us & 0xFFFFFFFF
        )
        
        # Scope of CRC is all bytes starting after sync bytes (from offset 2) up to end of payload
        crc_data = header[2:] + payload_data
        crc = calculate_crc16(crc_data)
        
        #Big endian wegene CRC16
        crc_bytes = struct.pack('>H', crc)
        
        return header + payload_data + crc_bytes

    async def broadcast_packet(self, packet: bytes):
        disconnected = set()
        for writer in self.clients:
            try:
                writer.write(packet)
                await writer.drain()
            except ConnectionError:
                disconnected.add(writer)
                
        for writer in disconnected:
            self.clients.remove(writer)

    async def replay_pcap(self, pcap_file: str):
        from scapy.all import PcapReader, Raw
        last_ts = None
        print(f"[*] Replaying PCAP: {pcap_file}")
        
        #Pcap Logik, (unfertig)
        with PcapReader(pcap_file) as pcap_reader:
            for pkt in pcap_reader:
                if Raw in pkt:
                    data = bytes(pkt[Raw])
                    
                    if last_ts is not None:
                        delay = float(pkt.time) - last_ts
                        if delay > 0:
                            await asyncio.sleep(delay)
                    last_ts = float(pkt.time)
                    
                    if self.clients:
                        await self.broadcast_packet(data)
        
        print("[*] PCAP replay finished.")

    async def generate_stream(self, payload_type_name: str, frequency: float, payload_generator):
        interval = 1.0 / frequency
        print(f"[*] Starting {payload_type_name} stream at {frequency} Hz")
        while True:
            payload_data = payload_generator()
            try:
                packet = self.build_packet(payload_type_name, payload_data)
                if self.clients:
                    await self.broadcast_packet(packet)
            except Exception as e:
                print(f"[!] Error generating packet {payload_type_name}: {e}")
            
            await asyncio.sleep(interval)
            
    async def run_tick_counter(self):
        while True:
            self.tick_counter += 1
            await asyncio.sleep(1.0 / 25.0)

def gen_btc_env() -> bytes:
    # PayloadBtcEnv - Type 0x10
    # Valid Mask (uint8), Reserved (uint8), TMP117 (int16), MS5611 Pres (uint32), MS5611 Temp (uint32)
    # ICM-42686 Accel (int16[3]), Gyro (int16[3])
    return struct.pack('<BBhIIhhhhhh',
        0x07, 0x00, 2500, 101325, 2000,
        random.randint(-100, 100), random.randint(-100, 100), random.randint(-100, 100),
        random.randint(-10, 10), random.randint(-10, 10), random.randint(-10, 10)
    )

def gen_btc_status() -> bytes:
    # PayloadBtcStatus - Type 0x11
    # Uptime (uint32), LO RTC (uint32), SD Status (uint8), REXUS (uint8), Reserved (uint8[2])
    return struct.pack('<IIBBBB', 3600, 120, 0x01, 0x01, 0x00, 0x00)

def gen_random_payload(size: int):
    return lambda: random.randbytes(size)

async def main():
    parser = argparse.ArgumentParser(description="Telemetry Sender")
    parser.add_argument('--pcap', type=str, help="PCAP file to replay (optional)")
    args = parser.parse_args()

    sender = TelemetrySender()
    
    server = await asyncio.start_server(
        sender.handle_client, '127.0.0.1', 5000
    )
    
    addr = server.sockets[0].getsockname()
    print(f'[*] Telemetry server listening on {addr}')

    tasks = []
    
    if args.pcap:
        tasks.append(asyncio.create_task(sender.replay_pcap(args.pcap)))
    else:
        tasks.append(asyncio.create_task(sender.run_tick_counter()))
        
        if 'BTC_ENV' in FREQUENCIES:
            tasks.append(asyncio.create_task(sender.generate_stream('BTC_ENV', FREQUENCIES['BTC_ENV'], gen_btc_env)))
            
        if 'BTC_STATUS' in FREQUENCIES:
            tasks.append(asyncio.create_task(sender.generate_stream('BTC_STATUS', FREQUENCIES['BTC_STATUS'], gen_btc_status)))

        for ptype, freq in FREQUENCIES.items():
            if ptype not in ['BTC_ENV', 'BTC_STATUS'] and ptype in PAYLOAD_SIZES:
                tasks.append(asyncio.create_task(
                    sender.generate_stream(ptype, freq, gen_random_payload(PAYLOAD_SIZES[ptype]))
                ))

    async with server:
        await asyncio.gather(server.serve_forever(), *tasks)

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[*] Server shutdown")
