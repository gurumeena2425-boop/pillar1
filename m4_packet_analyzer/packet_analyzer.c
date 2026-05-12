#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pcap.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#define ETHERNET_HEADER_SIZE 14

void print_mac(const unsigned char *mac) {
    printf("%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

const char *get_protocol_name(uint8_t protocol) {
    switch (protocol) {
        case IPPROTO_TCP:
            return "TCP";
        case IPPROTO_UDP:
            return "UDP";
        case IPPROTO_ICMP:
            return "ICMP";
        default:
            return "OTHER";
    }
}

const char *get_application_protocol(uint16_t source_port, uint16_t destination_port) {
    if (source_port == 53 || destination_port == 53) {
        return "DNS";
    } else if (source_port == 80 || destination_port == 80) {
        return "HTTP";
    } else if (source_port == 443 || destination_port == 443) {
        return "HTTPS";
    } else if (source_port == 22 || destination_port == 22) {
        return "SSH";
    } else if (source_port == 5000 || destination_port == 5000) {
        return "CUSTOM_TCP_TEST_SERVICE";
    } else if (source_port == 5001 || destination_port == 5001) {
        return "CUSTOM_UDP_TEST_SERVICE";
    } else {
        return "UNKNOWN";
    }
}

void analyze_packet(const unsigned char *packet, int packet_number, unsigned int captured_len) {
    if (captured_len < ETHERNET_HEADER_SIZE) {
        printf("Packet %d is too short for Ethernet header\n", packet_number);
        return;
    }

    struct ether_header *eth = (struct ether_header *)packet;

    printf("\n========== Packet %d ==========\n", packet_number);
    printf("Captured length          : %u bytes\n", captured_len);

    printf("Destination MAC          : ");
    print_mac(eth->ether_dhost);
    printf("\n");

    printf("Source MAC               : ");
    print_mac(eth->ether_shost);
    printf("\n");

    uint16_t ether_type = ntohs(eth->ether_type);

    printf("EtherType                : 0x%04x", ether_type);

    if (ether_type == ETHERTYPE_IP) {
        printf(" (IPv4)\n");
    } else if (ether_type == ETHERTYPE_ARP) {
        printf(" (ARP)\n");
        return;
    } else if (ether_type == ETHERTYPE_IPV6) {
        printf(" (IPv6)\n");
        return;
    } else {
        printf(" (Other)\n");
        return;
    }

    if (captured_len < ETHERNET_HEADER_SIZE + sizeof(struct ip)) {
        printf("Packet too short for IPv4 header\n");
        return;
    }

    struct ip *ip_header = (struct ip *)(packet + ETHERNET_HEADER_SIZE);

    int ip_header_len = ip_header->ip_hl * 4;

    if (captured_len < (unsigned int)(ETHERNET_HEADER_SIZE + ip_header_len)) {
        printf("Invalid IP header length\n");
        return;
    }

    char source_ip[INET_ADDRSTRLEN];
    char destination_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &(ip_header->ip_src), source_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip_header->ip_dst), destination_ip, INET_ADDRSTRLEN);

    printf("IP Source                : %s\n", source_ip);
    printf("IP Destination           : %s\n", destination_ip);
    printf("IP Header Length         : %d bytes\n", ip_header_len);
    printf("IP Total Length          : %d bytes\n", ntohs(ip_header->ip_len));
    printf("TTL                      : %d\n", ip_header->ip_ttl);
    printf("Protocol                 : %s (%d)\n",
           get_protocol_name(ip_header->ip_p),
           ip_header->ip_p);

    const unsigned char *transport_header =
        packet + ETHERNET_HEADER_SIZE + ip_header_len;

    unsigned int transport_offset =
        ETHERNET_HEADER_SIZE + ip_header_len;

    if (ip_header->ip_p == IPPROTO_TCP) {
        if (captured_len < transport_offset + sizeof(struct tcphdr)) {
            printf("Packet too short for TCP header\n");
            return;
        }

        struct tcphdr *tcp_header = (struct tcphdr *)transport_header;

        uint16_t source_port = ntohs(tcp_header->source);
        uint16_t destination_port = ntohs(tcp_header->dest);

        int tcp_header_len = tcp_header->doff * 4;

        printf("TCP Source Port          : %u\n", source_port);
        printf("TCP Destination Port     : %u\n", destination_port);
        printf("Application Protocol     : %s\n",
               get_application_protocol(source_port, destination_port));
        printf("TCP Header Length        : %d bytes\n", tcp_header_len);

        printf("TCP Flags                :");

        if (tcp_header->syn) {
            printf(" SYN");
        }

        if (tcp_header->ack) {
            printf(" ACK");
        }

        if (tcp_header->fin) {
            printf(" FIN");
        }

        if (tcp_header->rst) {
            printf(" RST");
        }

        if (tcp_header->psh) {
            printf(" PSH");
        }

        if (tcp_header->urg) {
            printf(" URG");
        }

        printf("\n");

        printf("Flow                     : %s:%u -> %s:%u TCP\n",
               source_ip,
               source_port,
               destination_ip,
               destination_port);
    }
    else if (ip_header->ip_p == IPPROTO_UDP) {
        if (captured_len < transport_offset + sizeof(struct udphdr)) {
            printf("Packet too short for UDP header\n");
            return;
        }

        struct udphdr *udp_header = (struct udphdr *)transport_header;

        uint16_t source_port = ntohs(udp_header->source);
        uint16_t destination_port = ntohs(udp_header->dest);
        uint16_t udp_length = ntohs(udp_header->len);

        printf("UDP Source Port          : %u\n", source_port);
        printf("UDP Destination Port     : %u\n", destination_port);
        printf("Application Protocol     : %s\n",
               get_application_protocol(source_port, destination_port));
        printf("UDP Length               : %u bytes\n", udp_length);

        printf("Flow                     : %s:%u -> %s:%u UDP\n",
               source_ip,
               source_port,
               destination_ip,
               destination_port);
    }
    else if (ip_header->ip_p == IPPROTO_ICMP) {
        printf("ICMP packet detected\n");

        printf("Flow                     : %s -> %s ICMP\n",
               source_ip,
               destination_ip);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <pcap_file>\n", argv[0]);
        return 1;
    }

    char errbuf[PCAP_ERRBUF_SIZE];

    pcap_t *handle = pcap_open_offline(argv[1], errbuf);

    if (handle == NULL) {
        fprintf(stderr, "Could not open pcap file: %s\n", errbuf);
        return 1;
    }

    printf("PCAP file opened successfully: %s\n", argv[1]);

    struct pcap_pkthdr *header;
    const unsigned char *packet;

    int packet_count = 0;
    int ipv4_count = 0;
    int tcp_count = 0;
    int udp_count = 0;
    int icmp_count = 0;
    int other_count = 0;

    while (pcap_next_ex(handle, &header, &packet) == 1) {
        packet_count++;

        if (header->caplen >= ETHERNET_HEADER_SIZE + sizeof(struct ip)) {
            struct ether_header *eth = (struct ether_header *)packet;

            if (ntohs(eth->ether_type) == ETHERTYPE_IP) {
                ipv4_count++;

                struct ip *ip_header =
                    (struct ip *)(packet + ETHERNET_HEADER_SIZE);

                if (ip_header->ip_p == IPPROTO_TCP) {
                    tcp_count++;
                } else if (ip_header->ip_p == IPPROTO_UDP) {
                    udp_count++;
                } else if (ip_header->ip_p == IPPROTO_ICMP) {
                    icmp_count++;
                } else {
                    other_count++;
                }
            } else {
                other_count++;
            }
        } else {
            other_count++;
        }

        analyze_packet(packet, packet_count, header->caplen);
    }

    printf("\n========== Summary ==========\n");
    printf("Total packets read : %d\n", packet_count);
    printf("IPv4 packets       : %d\n", ipv4_count);
    printf("TCP packets        : %d\n", tcp_count);
    printf("UDP packets        : %d\n", udp_count);
    printf("ICMP packets       : %d\n", icmp_count);
    printf("Other packets      : %d\n", other_count);

    pcap_close(handle);

    return 0;
}