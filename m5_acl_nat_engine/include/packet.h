cat > include/packet.h << 'EOF'
#ifndef PACKET_H
#define PACKET_H

#include <stdio.h>

typedef struct {
    char src_ip[32];
    char dst_ip[32];
    int src_port;
    int dst_port;
    char protocol[16];
    char in_iface[16];
    char out_iface[16];
} Packet;

void print_packet(Packet *pkt);
int read_packet_line(FILE *fp, Packet *pkt);

#endif
EOF
