import time
from sender import TelemetrySender, gen_btc_env, gen_btc_status, gen_random_payload, PAYLOAD_SIZES
# pyrefly: ignore [missing-import]
from scapy.all import Ether, IP, UDP, wrpcap

def main():
    sender = TelemetrySender()
    
    packets = []
    # Start time for simulating timestamps in the pcap
    current_time = time.time()
    
    print("[*] Generating 250 synthetic telemetry packets...")
    
    for i in range(250):
        # Mix of packets: 1 status for every 25 env packets
        if i % 25 == 0:
            ptype = 'BTC_STATUS'
            data = gen_btc_status()
        else:
            ptype = 'BTC_ENV'
            data = gen_btc_env()
            
        # Build the valid byte array using the existing simulation logic
        raw_bytes = sender.build_packet(ptype, data)
        
        # Wrap it in Ethernet/IP/UDP headers using scapy
        pkt = Ether()/IP(dst="127.0.0.1", src="127.0.0.1")/UDP(dport=5000, sport=12345)/raw_bytes
        
        # Override the packet timestamp to simulate 25Hz capture (40ms interval)
        pkt.time = current_time
        
        packets.append(pkt)
        current_time += 0.040 # Advance by 40ms
        
        # Also advance sender's tick counter manually since we aren't running its async loop
        sender.tick_counter += 1
        
    wrpcap('example_telemetry.pcap', packets)
    print("[+] Successfully generated 'example_telemetry.pcap' with 250 packets!")

if __name__ == '__main__':
    main()
