#ifndef NAT_H
#define NAT_H

#include "packet.h"

#define MAX_NAT_RULES 100
#define MAX_NAT_STATES 100

typedef struct {
    char type[16];
    char original_ip[32];
    char translated_ip[32];
} NATRule;

typedef struct {
    char original_src_ip[32];
    int original_src_port;
    char translated_src_ip[32];
    int translated_src_port;
    char dst_ip[32];
    int dst_port;
    char protocol[16];
} NATState;

int load_nat_rules(const char *filename, NATRule rules[]);
int apply_nat(Packet *pkt, NATRule rules[], int rule_count, NATState states[], int *state_count);
void print_nat_states(NATState states[], int state_count);

#endif