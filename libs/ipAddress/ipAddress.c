#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ws2tcpip.h>

#include "ipAddress.h"

uint32_t ipStrToInt(const char *ip_str) {
    return ntohl(inet_addr(ip_str));
}

void ipIntToStr(uint32_t ip, char *buffer) {
    struct in_addr addr;

    // Convert to network byte order
    addr.S_un.S_addr = htonl(ip);

    // Convert to string
    strcpy(buffer, inet_ntoa(addr));
}

int cidrToMaskStr(int cidr_prefix, char *mask_str) {
    if (cidr_prefix < 0 || cidr_prefix > 32) {
        return 1;
    }

    // Converts cidr prefix into a mask
    unsigned long mask = (cidr_prefix == 0) ? 0 : (~0UL << (32 - cidr_prefix));
    
    struct in_addr addr;

    // Convert to network byte order
    addr.S_un.S_addr = htonl(mask);  

    // Convert to string
    strcpy(mask_str, inet_ntoa(addr));  
    return 0;
}

int parseIpNetwork(char *cidr, Ipv4Network *network) {
    // find cidr mask
    char *cidr_prefix_str = strstr(cidr, "/");
    if(cidr_prefix_str == NULL) return 1;

    // create a string of just the ip
    size_t ip_len = cidr_prefix_str - cidr;
    char *ip_str = malloc(ip_len + 1);
    strncpy(ip_str, cidr, (cidr_prefix_str-cidr));

    // get the subnet mask
    int cidr_prefix = atoi(cidr_prefix_str+1);
    char mask_str[INET_ADDRSTRLEN];
    int res = cidrToMaskStr(cidr_prefix, mask_str);
    if(res) {
        printf("Invalid CIDR prefix length: %s\n", cidr_prefix_str);
        WSACleanup();
        return 1;
    }

    // populate the network information 
    uint32_t ip = ipStrToInt(ip_str);
    network->mask = ipStrToInt(mask_str);
    network->start = ip & network->mask;
    network->end = network->start | ~network->mask;

    return 0;
}