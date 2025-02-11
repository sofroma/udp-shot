#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <winsock2.h>
#include <stdio.h>

#define DEFAULT_BUFLEN 512
#define MAX_ADDR_LENGTH 255
#define MAX_PORT_LENGTH 5
#define MAX_INPUT_CHARACTERS 100

#define ERROR_TOO_LONG 2
#define ERROR_COPY 3

struct Resources {
	ADDRINFOW* targetAddr;
	ADDRINFOW* listenAddr;
	SOCKET sendSocket;
};

struct ParseArgParams {
	wchar_t* target;
	wchar_t* copyTo;
	int maxLength;
	char* out;
};


void dispose(struct Resources* resources);
void printUsage();
/*
Return values:
	0 - success
	1 - argkey and target are not equal
	2 - argval length is greater than maxLength
	3 - internal copy error
*/
char parseArgFor(wchar_t* argkey, wchar_t* argval, wchar_t* target, wchar_t* copyTo, int maxLength, char* out);
char parseArgForParams(wchar_t* argkey, wchar_t* argval, struct ParseArgParams params);


void printUsage()
{
	printf("Usage options:\n");
	printf("-addr ADDR - Address to send a packet\n");
	printf("-port PORT - Port\n\n");
	printf("Example: tool.exe -addr 192.168.0.2 -port 8080\n\n");
	printf("Optional options:\n");
	printf("-data DATA - String data to send. Max 100 characters.\n");
	printf("-listen PORT - Source port\n");
}

int wmain(int argc, wchar_t *argv[])
{
	if(argc < 5) {
		printUsage();
		return 0;
	}
	wchar_t addr[MAX_ADDR_LENGTH + 1];
	wchar_t port[MAX_PORT_LENGTH + 1];
	wchar_t userData[MAX_INPUT_CHARACTERS + 1];
	wchar_t listenPort[MAX_PORT_LENGTH + 1];
	
	char hasAddr = 0;
	char hasPort = 0;
	char hasUserData = 0;
	char hasListenPort = 0;

	struct ParseArgParams params[] = {
		{L"-addr", addr, MAX_ADDR_LENGTH, &hasAddr},
		{L"-port", port, MAX_PORT_LENGTH, &hasPort},
		{L"-data", userData, MAX_INPUT_CHARACTERS, &hasUserData},
		{L"-listen", listenPort, MAX_PORT_LENGTH, &hasListenPort}
	};

	for(int a = 1; a < argc; a++) {
		if(a + 1 < argc) {
			for(int p = 0; p < sizeof(params) / sizeof(params[0]); p++) {
				int parseResult = parseArgForParams(argv[a], argv[a + 1], params[p]);
				if(parseResult == ERROR_TOO_LONG || parseResult == ERROR_COPY)
				{
					printf("parse arguments failed\n");
					return 1;
				}
			}
		}
	}

	if(!hasAddr || !hasPort) {
		printUsage();
		return 1;
	}

	WSADATA wsaData;
	int iResult;
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}
	struct Resources resources;
	resources.targetAddr = NULL;
	resources.listenAddr = NULL;
	resources.sendSocket = INVALID_SOCKET;

	ADDRINFOW hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	iResult = GetAddrInfoW(addr, port, &hints, &resources.targetAddr);
	if(iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}
	resources.sendSocket = socket(resources.targetAddr->ai_family, resources.targetAddr->ai_socktype,
		resources.targetAddr->ai_protocol);
	if(resources.sendSocket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		dispose(&resources);
		return 1;
	}
	if(hasListenPort) {
		hints.ai_flags = AI_PASSIVE;
		iResult = GetAddrInfoW(NULL, listenPort, &hints, &resources.listenAddr);
		if(iResult != 0) {
			printf("getaddrinfo failed: %d\n", iResult);
			dispose(&resources);
			return 1;
		}
		iResult = bind(resources.sendSocket, resources.listenAddr->ai_addr, (int)resources.listenAddr->ai_addrlen);
		if(iResult == SOCKET_ERROR) {
			printf("bind failed: %d\n", WSAGetLastError());
			dispose(&resources);
			return 1;
		}
	}

	wchar_t *defaultData = L"12";
	int recvbuflen = DEFAULT_BUFLEN;
	char sendBuffer[DEFAULT_BUFLEN];
	wchar_t* data;
	
	if(hasUserData) {
		data = userData;
	} else {
		data = defaultData;
	}
	
	int dataStrLen = wcslen(data);
	int bytesCount = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, data,
		dataStrLen, sendBuffer, DEFAULT_BUFLEN, NULL, NULL);
	if(bytesCount == 0) {
		int convErrorCode = GetLastError();
		printf("UTF-8 conversion error code: %d\n", convErrorCode);
		if(convErrorCode == ERROR_NO_UNICODE_TRANSLATION) {
			printf("Invalid user input\n");
		}
		if(convErrorCode == ERROR_INSUFFICIENT_BUFFER) {
			printf("Buffer size too small\n");
		}
		dispose(&resources);
		return 1;
	}
	iResult = sendto(resources.sendSocket, sendBuffer, bytesCount, 0,
		resources.targetAddr->ai_addr, resources.targetAddr->ai_addrlen);
	if(iResult == SOCKET_ERROR) {
		printf("send failed: %d\n", WSAGetLastError());
		dispose(&resources);
		return 1;
	}
	printf("Bytes sent: %ld\n", iResult);
	dispose(&resources);
	return 0;
}

char parseArgFor(wchar_t* argkey, wchar_t* argval, wchar_t* target, wchar_t* copyTo, int maxLength, char* out)
{
	if(wcscmp(argkey, target) == 0) {
		if(wcslen(argval) > maxLength) {
			printf("argval for %ls is too long\n", target);
			return ERROR_TOO_LONG;
		}
		int cpResult = wcscpy_s(copyTo, maxLength + 1, argval);
		if(cpResult != 0) {
			printf("Invalid input for %ls\n", target);
			return ERROR_COPY;
		}
		*out = 1;
		return 0;
	} else {
		return 1;
	}
}

char parseArgForParams(wchar_t* argkey, wchar_t* argval, struct ParseArgParams params)
{
	return parseArgFor(argkey, argval, params.target, params.copyTo, params.maxLength, params.out);
}

void dispose(struct Resources* resources)
{
	if(resources->targetAddr != NULL) {
		FreeAddrInfoW(resources->targetAddr);
	}
	if(resources->listenAddr != NULL) {
		FreeAddrInfoW(resources->listenAddr);
	}
	if(resources->sendSocket != INVALID_SOCKET) {
		closesocket(resources->sendSocket);
	}
	WSACleanup();
}
