# M0 — Baseline Alignment and Linux Network Workbench Setup

## Industry Context
Network software engineers need a repeatable Linux workbench where socket programs, packet captures, interface inspection, route inspection, and socket debugging tools work correctly before building network product software.

## Objective
Validate the Linux/Docker environment for Pillar 1 Network Software Engineering work. The environment must support C compilation, socket programs, packet capture, and network inspection tools.

## Tools Validated
- gcc
- make
- git
- python3
- tcpdump
- tshark
- ip
- ss
- netstat
- Docker

## Evidence Generated
- reports/m0_environment_validation.txt
- captures/m0_baseline_tcp.pcap
- captures/m0_baseline_udp.pcap
- reports/m0_tcp_capture_summary.txt
- reports/m0_udp_capture_summary.txt

## Interface Used
Loopback interface `lo` was used because the TCP and UDP programs communicate using localhost / 127.0.0.1.

## TCP Capture Command
```bash
tcpdump -i lo -nn -w captures/m0_baseline_tcp.pcap 'tcp port 5000'
```
## UDP Capture Command
```bash
tcpdump -i lo -nn -w captures/m0_baseline_udp.pcap 'udp port 5001'
```