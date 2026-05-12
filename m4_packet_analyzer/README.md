# M4 — Packet Parsing and Protocol Interpretation

## Objective

The objective of M4 is to move from application-level socket communication to actual packet-level understanding.

This project builds a Linux Network Packet Analyzer and Flow Inspector using C and libpcap.

## What the analyzer does

The analyzer reads a `.pcap` file and extracts:

- Ethernet source and destination MAC address
- EtherType
- IPv4 source and destination address
- IP header length
- IP total length
- TTL
- Protocol type: TCP, UDP, ICMP
- TCP source and destination ports
- UDP source and destination ports
- TCP flags: SYN, ACK, FIN, RST, PSH, URG
- Application protocol classification based on port numbers
- Flow summary

## Build

```bash
make clean
make