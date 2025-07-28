# WinSock PortScanner
This is a relatively basic tcp port scanner. I programmed it to re-learn C, so it probably won't contain best practices.
This port scanner is multithreaded, so as long as all of the ports are open, then it will be fast. Because it uses connect(), which is typically a blocked function, it immediately stops the current thread until it is closed or it times out.

## How to use
1. Download the source code and run `make`
2. Then run `scanner.exe` and use the following arguments

| Argument | Shorthand | Example | Description |
| -- | -- | -- | -- |
| ipaddress | ip | -ip 192.168.0.0/26,192.168.100.0-192.168.100.2 | IP, CIDR IP network, IP Range, or a comma separated list of either (REQUIRED) |
| port | p | -p 80,443-445 | port, comma separated list of ports, or an inclusive range or ports. (Default: 1-1024) |
| thread | t | -t 2 | number of threads to scan everything. (Default: 1) |

## Future Progress
I might try and re-write this to utilize npcap so that I can do more than just TCP scans.