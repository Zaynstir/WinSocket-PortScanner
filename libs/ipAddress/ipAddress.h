#pragma once

// A struct to convey an ipv4 network
typedef struct {
    uint32_t start;
    uint32_t end;
    uint32_t mask;
} Ipv4Network;

/*
    Convert string IP to uint32_t

    params
        [const char *] ip_str - the ip as a string

    returns
        [uint32_t] - ip as a string
*/ 
uint32_t ipStrToInt(const char *ip_str);

/*
    Convert uint32_t to string IP

    params
        [uint32_t] ip - ip as an int
        (out)[char *] buffer - a passed in string to be written on
*/
void ipIntToStr(uint32_t ip, char *buffer);

/*
    Convert CIDR to subnet mask string (e.g., /24 -> 255.255.255.0)

    params
        [int] cidr_prefix - the cidr prefix
        (out)[char *] buffer - a passed in string to be written on

    returns
        [int] - 0 is success, 1 is an error occurred
*/
int cidrToMaskStr(int cidr_prefix, char *mask_str);

/*
    Will populate the Ipv4Network struct based on the provided cidr string

    params
        [char *] cidr - A cidr notated network string
        (out)[Ipv4Network *] network - an empty struct for network info

    returns
        [int] - 0 is success, 1 is an error occurred
*/
int parseIpNetwork(char *cidr, Ipv4Network *network);