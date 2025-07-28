#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <string.h>

#include "libs/pargs/pargs.h"
#include "libs/ipAddress/ipAddress.h"
#include "libs/stringFunctions/stringFuncs.h"

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#define DEFAULT_BUFLEN 512

// Enum of port state. There's probably a better way to handle this, but we'll keep it like this for now.
typedef enum {
    CLOSED,
    FILTERED,
    OPEN
} portState;

// Struct to record a port range (1-1024)
//  - this is to prevent the need for a giant list of ports
typedef struct {
    int port;
    int count;
} portRange;

// Struct to record an ip subnet or range (192.168.0.0/24)
//  - this is to prevent the need for a giant list of ips
typedef struct {
    uint32_t ip;
    int count;
} ipRange;

// Keeps track of the ips/ports to scan, and the global indexes used among threads
typedef struct {
    ipRange *ipRanges;
    int ipRangeCount;
    portRange *portRanges;
    int portRangeCount;
    int ipRangeIdx;
    int ipIdx;
    int portRangeIdx;
    int portIdx;
    int done;
} scanIndexes;

// An object passed to threads which handles all the special parameters that someone would pass
typedef struct {
    int threads;
    scanIndexes index;
} psConfig;

// the results for a given port
typedef struct {
    uint32_t port;
    portState state;
    char *service;
} portResult;

// the results of an ip and the list of port results
typedef struct {
    uint32_t ip;
    char ip_str[INET_ADDRSTRLEN];
    portResult **ports;
    int portCount;
    int portCapacity;
} ipResult;

// a list of all ip results
typedef struct {
    ipResult **ips;
    int count;
    int capacity;
} scanResults;

// the struct passed to threads with every necessary parameter
struct mt_ipScanStruct {
    int threadId;
    psConfig *config;
    scanResults *results;
    CRITICAL_SECTION indexLock;
    CRITICAL_SECTION resultLock;
};


/*
    Performs a TCP socket connection to determine the state of a port at the provided ip, then stores it into pResult

    params
        [uint32_t] ip - the ip to scan (written as in int)
        [uint32_t] port - the port to scan
        [struct addrinfo *] hints - addrinfo for tcp sockets
        (out)[portResult] pResult - an empty port result struct
    
    returns
        [int] - 0 is success, 1 is an error occurred
*/
int scanPort(uint32_t ip, uint32_t port, struct addrinfo *hints, portResult *pResult){
    SOCKET sock = INVALID_SOCKET;
    struct addrinfo *result = NULL;
    
    char ip_str[INET_ADDRSTRLEN];
    ipIntToStr(ip, ip_str);
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    printf(" - %s:%s\n", ip_str, port_str);
    
    // Setup addrinfo
    int res;    
    if((res = getaddrinfo(ip_str, port_str, hints, &result))) {
        printf("Failed to get getaddrinfo: %d\n", res);
        return 1;
    }

    // initialize sock
    sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if(sock == INVALID_SOCKET) {
        printf("Err: Failed to create socket: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        return 1;
    }

    // attempt connection to get port state
    if((res = connect(sock, result->ai_addr, (int) result->ai_addrlen))) {
        int err = WSAGetLastError();
        if(err == WSAECONNREFUSED) {
            pResult->state = CLOSED;
        } else if (err == WSAETIMEDOUT) {
            pResult->state = FILTERED;
        } else {
            printf("Err: Failed to connect: %d\n", WSAGetLastError());
            closesocket(sock);
            freeaddrinfo(result);
            return 1;
        }
    } else {
        pResult->state = OPEN;
    }
    
    return 0;
}

/*
    Adds a portResult to the scanResults passed to the thread

    params
        (out)[scanResults *] results - results of all scans
        [uint32_t] ip - the ip where the pResult was going to be stored
        [portResult] pResult - the port scan results
    
    returns
        [int] - 0 is success, 1 is an error occurred
*/
int addScanResult(scanResults *results, uint32_t ip, portResult *port) {
    // determine the index in which ip exists in results
    int ipIndex;
    for(ipIndex = 0; ipIndex < results->count; ipIndex++) {
        if(results->ips[ipIndex]->ip == ip) {
            break;
        }
    }
    
    // if ip doesn't exist in results, then we need to append it to the scanResults
    if(ipIndex == results->count) {
        // reallocate if necessary
        if(results->count == results->capacity) {
            ipResult *newIpRay = realloc(results->ips, (results->capacity * 2 * sizeof(ipResult*)));
            if(newIpRay == NULL) {
                printf("Err: Failed to re-allocate memory\n");
                return 1;
            }
            results->capacity *= 2;
        }
        
        // malloc ipResult to keep track or port scan results
        ipResult *result = malloc(sizeof(ipResult));
        if(!result) {
            printf("Err: Failed to allocate memory for ipResult\n");
            return 1;
        }
        result->ip = ip;
        ipIntToStr(ip, result->ip_str);
        result->portCount = 0;
        result->portCapacity = 2;
        result->ports = malloc(result->portCapacity * sizeof(portResult *));

        results->ips[results->count++] = result;
    }

    // if not enough space in portResults of ipResult, then realloc to add more space
    ipResult *result = results->ips[ipIndex];
    if(result->portCount == result->portCapacity) {
        portResult **newPortRay = realloc(result->ports, (result->portCapacity * 2 * sizeof(portResult*)));
        if(newPortRay == NULL) {
            printf("Err: Failed to allocate memory for portResult.\n");
            return 1;
        }
        result->portCapacity *= 2;
        result->ports = newPortRay;
    }

    // Add pResult to provided ipIndex
    result->ports[result->portCount++] = port;

    return 0;
}

/*
    Gets the next port/ip for the current thread's scan

    params
        [scanIndexes *] index - a global index variables to keep track of the current index across threads
*/
void getNext(scanIndexes *index) {    
    // if no more indexes to handle return to exit thread
    if(index->done) return;
    
    // If at last port in current portRange, then go to start of next portRange
    if(++index->portIdx >= index->portRanges[index->portRangeIdx].count) {
        index->portRangeIdx++;
        index->portIdx = 0;
    }

    // If at last port range struct, then go to next ip in ip range struct and start at first port range struct
    if(index->portRangeIdx >= index->portRangeCount) {
        index->ipIdx++;
        index->portRangeIdx = 0;
    }

    // If at last ip in ip range struct, then got to next ip range struct
    if(index->ipIdx >= index->ipRanges[index->ipRangeIdx].count) {
        index->ipRangeIdx++;
        index->ipIdx = 0;
    }

    // If at last ip range struct, then we can assume we have checked everything
    if(index->ipRangeIdx >= index->ipRangeCount) {
        index->done = 1;
    }
}

/*
    A multithreaded function to handle scanning ips and ports as provided in psConfig within the mt_ipScanStruct

    params
        [LPVOID] lpParam - the mt_ipScanStruct containing psConfig, results, and mutex locks
*/
DWORD WINAPI scan(LPVOID lpParam) {
    struct mt_ipScanStruct *params = (struct mt_ipScanStruct *)lpParam;

    // abstraction as it saves me from writing params infront of everything
    psConfig *config = params->config;

    // create the addrinfo hints with TCP sockets
    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int i = 0;
    while(config->index.done == 0) {
        int ipRangeIdx, ipIdx, portRangeIdx, portIdx;

        // grab mutex lock to get next index from global struct
        EnterCriticalSection(&params->indexLock);
        
        getNext(&config->index);
        
        // assign local copies, otherwise we introduce race conditions
        ipRangeIdx = config->index.ipRangeIdx;
        ipIdx = config->index.ipIdx;
        portRangeIdx = config->index.portRangeIdx;
        portIdx = config->index.portIdx;

        LeaveCriticalSection(&params->indexLock);

        // the last valid check sets index.done = 1, so we can't check this before the while loop
        if(config->index.done) {
            return 0;
        }

        // abstraction for ip and port
        int port = params->config->index.portRanges[portRangeIdx].port + portIdx;
        int ip = params->config->index.ipRanges[ipRangeIdx].ip + ipIdx;

        // pResult will store the result of the port scan
        portResult *pResult = malloc(sizeof(portResult *));
        if(pResult == NULL) {
            printf("Err: Failed to allocate memory for pResult\n");
            return 1;
        }
        pResult->port = port;
        
        int res = scanPort(ip, port, &hints, pResult);
        if(res) {
            return 1;
        }

        // We only want to add the scan result if the port is accessible, otherwise it becomes noisy
        if(pResult->state == OPEN || pResult->state == FILTERED) {
            // mutex lock for adding pResult to global scan results 
            EnterCriticalSection(&params->resultLock);
            
            addScanResult(params->results, ip, pResult);

            LeaveCriticalSection(&params->resultLock);
        }

        i++;
    }

    return 0;
}

/*
    Parse the -port flag argument and stores it into index->portRanges

    params
        [pargArray *] pargs - list of parsed arguments
        (out)[scanIndexes *] index - global variable used to handle portRanges
    
    returns
        [int] - 0 is success, 1 is an error occurred
*/
int parsePorts(pargArray *pargs, scanIndexes *index) {
    char *portString = getValue("port", pargs);

    // if port arg isn't provided then use the default value. 
    // - there's definitely a better way, but this is simpler
    if(portString == NULL) {
        portString = "1-1024";
    }

    // split portString based on commas if they exist
    stringArray ray;
    if(split(portString, ",", &ray)) {
        printf("Err: Failed to split port argument: %s", portString);
        return 1;
    }
    index->portRanges = malloc(ray.count * sizeof(portRange));
    index->portRangeCount = ray.count;
    for(int i = 0; i < ray.count; i++) {
        // split portString item based on dashes (this indicates a range)
        stringArray subRay;
        if(split(ray.items[i], "-", &subRay)) {
            printf("Err: Failed to split port option: %s", ray.items[i]);
            free(index->portRanges);
            return 1;
        }

        index->portRanges[i].port = atoi(subRay.items[0]);
        
        // if only a single port
        if (subRay.count == 1) {
            index->portRanges[i].count = 1;
        }
        // if a port range
        else if(subRay.count == 2) {
            index->portRanges[i].count = atoi(subRay.items[1]) - index->portRanges[i].port;
        } 
        // if an invalid portString item
        else {
            printf("Err: Too many delimeters: %s", ray.items[i]);
            free(index->portRanges);
            return 1;
        }

        // quick check to validate port number
        if(
            index->portRanges[i].port < 1 ||
            index->portRanges[i].port > (index->portRanges[i].port + index->portRanges[i].count) ||
            (index->portRanges[i].port + index->portRanges[i].count) > 65535
        ) {
            printf("Err: Port range is not valid: %d - %d", index->portRanges[i].port, index->portRanges[i].port + index->portRanges[i].count);
            free(index->portRanges);
            return 1;
        }
    }
    return 0;
}

/*
    Parse the -ip flag argument and stores it into index->ipRanges

    params
        [pargArray *] pargs - list of parsed arguments
        (out)[scanIndexes *] index - global variable used to handle ipRanges
    
    returns
        [int] - 0 is success, 1 is an error occurred
*/
int parseIps(pargArray *pargs, scanIndexes *index) {
    char *ipString = getValue("ipaddress", pargs);
    if(ipString == NULL) {
        printf("Err: ip is required");
        return 1;
    }

    // split ipString based on commas if they exist
    stringArray ray;
    if(split(ipString, ",", &ray)) {
        printf("Err: Failed to split ip argument: %s", ipString);
        return 1;
    }

    index->ipRanges = malloc(ray.count * sizeof(ipRange));
    index->ipRangeCount = ray.count;
    for(int i = 0; i < ray.count; i++) {
        // If an inclusive ip range
        char *rangePointer = strchr(ray.items[i], '-');
        if(rangePointer != NULL) {
            // then create an ipRange with the provided range
            stringArray rangeRay;
            split(ray.items[i], "-", &rangeRay);
            uint32_t firstIp = ipStrToInt(rangeRay.items[0]);
            uint32_t secondIp = ipStrToInt(rangeRay.items[1]);

            index->ipRanges[i].ip = firstIp;
            index->ipRanges[i].count = secondIp - firstIp;
        }
        
        // If a subnetmask
        char *maskPointer = strchr(ray.items[i], '/');
        if(maskPointer != NULL) {
            // The remaining valid option is that we were provided a subnet
            // Parse the subnet string into a network
            Ipv4Network network;
            parseIpNetwork(ray.items[i], &network);

            // Determine the ipRange
            index->ipRanges[i].ip = network.start;
            index->ipRanges[i].count = network.end - network.start;
        }

        // The only remaining option: a single ip address
        // - so just a single count ipRange
        index->ipRanges[i].ip = ipStrToInt(ray.items[i]);
        index->ipRanges[i].count = 1;
        continue;
    }
    return 0;
}

/*
    turns the parsed arguments into the config

    params
        [pargArray *] pargs - the parsed arguments
        [psConfig *] config - the global variable used to keep track of parameters
    
    returns
        [int] - 0 is success, 1 is an error occurred
*/
int configureConfig (pargArray *pargs, psConfig *config) {
    // initialize variables
    //  - setting portIdx to -1 is needed so that when we call getNext(), it gets the first index
    config->index.ipIdx = 0;
    config->index.ipRangeIdx = 0;
    config->index.portRangeIdx = 0;
    config->index.portIdx = -1;
    config->index.done = 0;

    // parse arguments into index
    parseIps(pargs, &config->index);
    parsePorts(pargs, &config->index);

    // Print ip/port counts
    int totalIps = 0;
    int totalPorts = 0;
    for(int i = 0; i < config->index.ipRangeCount; i++) {
        totalIps += config->index.ipRanges[i].count;
    }
    for(int i = 0; i < config->index.portRangeCount; i++) {
        totalPorts += config->index.portRanges[i].count;
    }
    printf("Scan Counts:\n- Ips: %d\n- Ports: %d\n", totalIps, totalPorts);

    return 0;
}



int main(int argc, char *argv[]) {
    pargOption options[] = {
        {"ipaddress","ip","IP, CIDR IP network, or a comma separated list of either", REQUIRED},
        {"port","p", "port, comma separated list of ports, or an inclusive range or ports (123-234). Default: 1-1024", 0},
        {"thread","t","number of threads to scan everything. Default: 1", 0},
        {NULL}
    };

    pargArray *pargs = parseArgs(argc, argv, options);
    printPargs(pargs);

    psConfig config;
    configureConfig(pargs, &config);

    // Setup mutex locks for multithreading
    CRITICAL_SECTION indexLock;
    CRITICAL_SECTION resultLock;
    InitializeCriticalSection(&indexLock); 
    InitializeCriticalSection(&resultLock); 

    // Setup results struct
    scanResults results;
    results.ips = malloc(sizeof(ipResult *));
    results.count = 0;
    results.capacity = 1;

    // setup WSA
    WSADATA wsa;
    int res;
    if((res = WSAStartup(MAKEWORD(2,2), &wsa))) {
        printf("WSAStartup failed: %d\n", res);
        return 1;
    }

    // get thread count
    char *threadCntStr = getValue("thread", pargs);
    int threadCnt = 1; // assign default of 1
    if(threadCntStr != NULL) {
        threadCnt = atoi(threadCntStr);
    }
    if(threadCnt < 1) {
        printf("Err: Provided thread count is less than 1.\n");
        return 1;
    }
    
    printf("============\nSCAN PROCESS\n============\n");
    // setup threads
    HANDLE *threads = malloc(threadCnt * sizeof(HANDLE));
    for(int i = 0; i < threadCnt; i++){
        struct mt_ipScanStruct mt_var;
        mt_var.config = &config;
        mt_var.results = &results;
        mt_var.indexLock = indexLock;
        mt_var.resultLock = resultLock;
        mt_var.threadId = i;

        threads[i] = CreateThread(
            NULL,
            0,
            scan,
            &mt_var,
            0,
            NULL
        );

        if(threads[i] == NULL) {
            printf("Failed to create thread %d\n", i);
            DeleteCriticalSection(&indexLock);
            DeleteCriticalSection(&resultLock);
            return 1;
        }
    }

    WaitForMultipleObjects(threadCnt, threads, TRUE, INFINITE);

    for(int i = 0; i < 2; i++) {
        CloseHandle(threads[i]);
    }
    DeleteCriticalSection(&indexLock);
    DeleteCriticalSection(&resultLock);

    printf("============\nSCAN RESULTS\n============\nIP Count: %d\n", results.count);
    for(int i = 0; i < results.count; i++) {
        ipResult *ipResult = results.ips[i];
        printf("IP: %s\n", ipResult->ip_str);
        printf("  Ports (%d):\n", ipResult->portCount);
        for(int k = 0; k < ipResult->portCount; k++){
            portResult *pResult = ipResult->ports[k];
            printf("   - %d: ", pResult->port);
            switch(pResult->state) {
                case OPEN:
                    printf("OPEN");
                    break;
                case FILTERED:
                    printf("FILTERED");
                    break;
                case CLOSED:
                    printf("CLOSED");
                    break;
            }
            printf("\n");
        }
    }

    printf("Done\n");
}