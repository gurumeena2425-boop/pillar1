#!/bin/bash

echo "===== M0 Environment Validation ====="

echo
echo "[1] Tool Versions"
gcc --version | head -1
make --version | head -1
git --version
python3 --version
tcpdump --version | head -1
tshark --version | head -1
ip -V
ss -V
netstat --version | head -1

echo
echo "[2] Interface Information"
ip addr

echo
echo "[3] Route Table"
ip route

echo
echo "[4] Active TCP/UDP Sockets"
ss -tanup

echo
echo "M0 validation completed successfully."
