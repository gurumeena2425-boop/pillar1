#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <strings.h>

#define MAX_ACL 100
#define MAX_NAT 20
#define MAX_ROUTE 50
#define MAX_NAT_STATE 100
#define LINE_SIZE 512

typedef struct {
    char action[16];
    char proto[16];

    int src_any;
    uint32_t src_ip;

    int src_port_any;
    int src_port;

    int dst_any;
    uint32_t dst_ip;

    int dst_port_any;
    int dst_port;
} ACLRule;

typedef struct {
    uint32_t private_net;
    int prefix;
    uint32_t public_ip;
} NATRule;

typedef struct {
    uint32_t net;
    int prefix;
    char iface[32];
} RouteRule;

typedef struct {
    int used;
    char proto[16];

    uint32_t private_ip;
    int private_port;

    uint32_t public_ip;
    int public_port;

    uint32_t remote_ip;
    int remote_port;
} NATState;

typedef struct {
    int id;
    char direction[8];
    char proto[16];

    uint32_t src_ip;
    int src_port;

    uint32_t dst_ip;
    int dst_port;
} Packet;

ACLRule acl_rules[MAX_ACL];
NATRule nat_rules[MAX_NAT];
RouteRule route_rules[MAX_ROUTE];
NATState nat_table[MAX_NAT_STATE];

int acl_count = 0;
int nat_count = 0;
int route_count = 0;
int nat_state_count = 0;

int next_public_port = 40000;

uint32_t ip_to_int(const char *ip_str) {
    unsigned int a, b, c, d;

    if (sscanf(ip_str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        fprintf(stderr, "Invalid IP address: %s\n", ip_str);
        exit(1);
    }

    return (a << 24) | (b << 16) | (c << 8) | d;
}

void ip_to_str(uint32_t ip, char *buf) {
    sprintf(buf, "%u.%u.%u.%u",
            (ip >> 24) & 0xff,
            (ip >> 16) & 0xff,
            (ip >> 8) & 0xff,
            ip & 0xff);
}

uint32_t prefix_mask(int prefix) {
    if (prefix == 0) {
        return 0;
    }
    return 0xffffffff << (32 - prefix);
}

int cidr_match(uint32_t ip, uint32_t net, int prefix) {
    uint32_t mask = prefix_mask(prefix);
    return (ip & mask) == (net & mask);
}

void parse_cidr(const char *cidr, uint32_t *net, int *prefix) {
    char ip_part[64];

    if (sscanf(cidr, "%63[^/]/%d", ip_part, prefix) != 2) {
        fprintf(stderr, "Invalid CIDR: %s\n", cidr);
        exit(1);
    }

    *net = ip_to_int(ip_part);
}

int parse_port(const char *s, int *is_any) {
    if (strcasecmp(s, "ANY") == 0) {
        *is_any = 1;
        return -1;
    }

    *is_any = 0;
    return atoi(s);
}

uint32_t parse_ip_or_any(const char *s, int *is_any) {
    if (strcasecmp(s, "ANY") == 0) {
        *is_any = 1;
        return 0;
    }

    *is_any = 0;
    return ip_to_int(s);
}

void load_rules(const char *filename) {
    FILE *fp = fopen(filename, "r");

    if (!fp) {
        perror("Failed to open rules file");
        exit(1);
    }

    char line[LINE_SIZE];

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        char type[32];
        sscanf(line, "%31s", type);

        if (strcasecmp(type, "ACL") == 0) {
            char action[16], proto[16];
            char src_ip[64], src_port[32], dst_ip[64], dst_port[32];

            int n = sscanf(line, "%*s %15s %15s %63s %31s %63s %31s",
                           action, proto, src_ip, src_port, dst_ip, dst_port);

            if (n != 6) {
                fprintf(stderr, "Invalid ACL rule: %s\n", line);
                exit(1);
            }

            ACLRule *r = &acl_rules[acl_count++];

            strcpy(r->action, action);
            strcpy(r->proto, proto);

            r->src_ip = parse_ip_or_any(src_ip, &r->src_any);
            r->src_port = parse_port(src_port, &r->src_port_any);

            r->dst_ip = parse_ip_or_any(dst_ip, &r->dst_any);
            r->dst_port = parse_port(dst_port, &r->dst_port_any);
        }

        else if (strcasecmp(type, "NAT") == 0) {
            char cidr[64], public_ip[64];

            int n = sscanf(line, "%*s %63s %63s", cidr, public_ip);

            if (n != 2) {
                fprintf(stderr, "Invalid NAT rule: %s\n", line);
                exit(1);
            }

            NATRule *r = &nat_rules[nat_count++];

            parse_cidr(cidr, &r->private_net, &r->prefix);
            r->public_ip = ip_to_int(public_ip);
        }

        else if (strcasecmp(type, "ROUTE") == 0) {
            char cidr[64], iface[32];

            int n = sscanf(line, "%*s %63s %31s", cidr, iface);

            if (n != 2) {
                fprintf(stderr, "Invalid ROUTE rule: %s\n", line);
                exit(1);
            }

            RouteRule *r = &route_rules[route_count++];

            parse_cidr(cidr, &r->net, &r->prefix);
            strcpy(r->iface, iface);
        }
    }

    fclose(fp);
}

int match_acl_rule(ACLRule *r, Packet *p) {
    if (strcasecmp(r->proto, "ANY") != 0 &&
        strcasecmp(r->proto, p->proto) != 0) {
        return 0;
    }

    if (!r->src_any && r->src_ip != p->src_ip) {
        return 0;
    }

    if (!r->src_port_any && r->src_port != p->src_port) {
        return 0;
    }

    if (!r->dst_any && r->dst_ip != p->dst_ip) {
        return 0;
    }

    if (!r->dst_port_any && r->dst_port != p->dst_port) {
        return 0;
    }

    return 1;
}

const char *evaluate_acl(Packet *p, int *matched_rule_index) {
    for (int i = 0; i < acl_count; i++) {
        if (match_acl_rule(&acl_rules[i], p)) {
            *matched_rule_index = i;
            return acl_rules[i].action;
        }
    }

    *matched_rule_index = -1;
    return "DENY";
}

NATRule *find_nat_rule_for_source(uint32_t src_ip) {
    for (int i = 0; i < nat_count; i++) {
        if (cidr_match(src_ip, nat_rules[i].private_net, nat_rules[i].prefix)) {
            return &nat_rules[i];
        }
    }

    return NULL;
}

void apply_outbound_nat(Packet *p, FILE *logfp) {
    NATRule *rule = find_nat_rule_for_source(p->src_ip);

    if (!rule) {
        fprintf(logfp, "NAT: no NAT rule matched, packet unchanged\n");
        return;
    }

    NATState *state = &nat_table[nat_state_count++];

    state->used = 1;
    strcpy(state->proto, p->proto);

    state->private_ip = p->src_ip;
    state->private_port = p->src_port;

    state->public_ip = rule->public_ip;
    state->public_port = next_public_port++;

    state->remote_ip = p->dst_ip;
    state->remote_port = p->dst_port;

    char private_ip[32], public_ip[32];

    ip_to_str(state->private_ip, private_ip);
    ip_to_str(state->public_ip, public_ip);

    fprintf(logfp,
            "NAT OUT: %s:%d translated to %s:%d\n",
            private_ip,
            state->private_port,
            public_ip,
            state->public_port);

    p->src_ip = state->public_ip;
    p->src_port = state->public_port;
}

int apply_inbound_nat(Packet *p, FILE *logfp) {
    for (int i = 0; i < nat_state_count; i++) {
        NATState *s = &nat_table[i];

        if (!s->used) {
            continue;
        }

        if (strcasecmp(s->proto, p->proto) == 0 &&
            s->public_ip == p->dst_ip &&
            s->public_port == p->dst_port &&
            s->remote_ip == p->src_ip &&
            s->remote_port == p->src_port) {

            char public_ip[32], private_ip[32];

            ip_to_str(s->public_ip, public_ip);
            ip_to_str(s->private_ip, private_ip);

            fprintf(logfp,
                    "NAT IN: %s:%d translated back to %s:%d\n",
                    public_ip,
                    s->public_port,
                    private_ip,
                    s->private_port);

            p->dst_ip = s->private_ip;
            p->dst_port = s->private_port;

            return 1;
        }
    }

    fprintf(logfp, "NAT IN: no matching NAT state found\n");
    return 0;
}

const char *route_lookup(uint32_t dst_ip) {
    int best_prefix = -1;
    const char *best_iface = "NO_ROUTE";

    for (int i = 0; i < route_count; i++) {
        if (cidr_match(dst_ip, route_rules[i].net, route_rules[i].prefix)) {
            if (route_rules[i].prefix > best_prefix) {
                best_prefix = route_rules[i].prefix;
                best_iface = route_rules[i].iface;
            }
        }
    }

    return best_iface;
}

int parse_packet_line(char *line, Packet *p) {
    char src_ip[64], dst_ip[64];

    int n = sscanf(line,
                   "%d,%7[^,],%15[^,],%63[^,],%d,%63[^,],%d",
                   &p->id,
                   p->direction,
                   p->proto,
                   src_ip,
                   &p->src_port,
                   dst_ip,
                   &p->dst_port);

    if (n != 7) {
        return 0;
    }

    p->src_ip = ip_to_int(src_ip);
    p->dst_ip = ip_to_int(dst_ip);

    return 1;
}

void print_packet(FILE *fp, const char *label, Packet *p) {
    char src[32], dst[32];

    ip_to_str(p->src_ip, src);
    ip_to_str(p->dst_ip, dst);

    fprintf(fp,
            "%s: %s %s:%d -> %s:%d direction=%s\n",
            label,
            p->proto,
            src,
            p->src_port,
            dst,
            p->dst_port,
            p->direction);
}

void process_packets(const char *input_file, const char *output_file) {
    FILE *in = fopen(input_file, "r");

    if (!in) {
        perror("Failed to open packet input file");
        exit(1);
    }

    FILE *logfp = fopen(output_file, "w");

    if (!logfp) {
        perror("Failed to open output log file");
        exit(1);
    }

    char line[LINE_SIZE];

    fgets(line, sizeof(line), in);

    while (fgets(line, sizeof(line), in)) {
        Packet p;

        if (!parse_packet_line(line, &p)) {
            continue;
        }

        fprintf(logfp, "====================================================\n");
        fprintf(logfp, "PACKET ID: %d\n", p.id);

        print_packet(logfp, "ORIGINAL", &p);

        if (strcasecmp(p.direction, "IN") == 0) {
            int nat_ok = apply_inbound_nat(&p, logfp);

            if (!nat_ok) {
                fprintf(logfp, "FINAL DECISION: DROP\n");
                fprintf(logfp, "DROP REASON: no NAT state for incoming packet\n");
                continue;
            }
        }

        int matched_rule = -1;
        const char *acl_action = evaluate_acl(&p, &matched_rule);

        if (matched_rule >= 0) {
            fprintf(logfp,
                    "ACL: matched rule %d action=%s\n",
                    matched_rule + 1,
                    acl_action);
        } else {
            fprintf(logfp, "ACL: no rule matched, default DENY\n");
        }

        if (strcasecmp(acl_action, "DENY") == 0) {
            fprintf(logfp, "FINAL DECISION: DROP\n");
            fprintf(logfp, "DROP REASON: ACL denied packet\n");
            continue;
        }

        if (strcasecmp(p.direction, "OUT") == 0) {
            apply_outbound_nat(&p, logfp);
        }

        const char *iface = route_lookup(p.dst_ip);

        if (strcasecmp(iface, "NO_ROUTE") == 0) {
            fprintf(logfp, "FINAL DECISION: DROP\n");
            fprintf(logfp, "DROP REASON: no route found\n");
            continue;
        }

        print_packet(logfp, "AFTER POLICY", &p);

        fprintf(logfp, "ROUTE: output interface = %s\n", iface);
        fprintf(logfp, "FINAL DECISION: FORWARD\n");
    }

    fclose(in);
    fclose(logfp);
}

int main() {
    printf("M5 ACL/NAT Rule Engine starting...\n");

    load_rules("config/rules.conf");

    printf("Loaded %d ACL rules\n", acl_count);
    printf("Loaded %d NAT rules\n", nat_count);
    printf("Loaded %d ROUTE rules\n", route_count);

    process_packets("input/packets.csv", "output/decision_log.txt");

    printf("Processing complete.\n");
    printf("Check output/decision_log.txt\n");

    return 0;
}