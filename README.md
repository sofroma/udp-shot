# UDP Shot

UDP packet sender tool for Windows.

### How to build

Execute Native Tools command prompt for Visual Studio and call:
```
cl.exe main.c /link Ws2_32.lib
```

### Usage

Usage options:\
-addr ADDR - Address to send a packet\
-port PORT - Port\
Optional options:\
	-data DATA - String data to send. Max 100 characters.\
	-listen PORT - Source port
```
Example: main.exe -addr 192.168.0.2 -port 27015
```
