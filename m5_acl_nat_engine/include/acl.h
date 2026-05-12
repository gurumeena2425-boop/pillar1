#ifndef ACL_H
#define ACL_H

#include "packet.h"

#define MAX_ACL_RULES 100

typedef struct {
    char action[16];
    char src_ip[32];
    char dst_ip[32];
    char protocol[16];
    char dst_port[16];
} ACLRule;

int load_acl_rules(const char *filename, ACLRule rules[]);
int evaluate_acl(Packet *pkt, ACLRule rules[], int rule_count, int *matched_rule_index);

#endif
