#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <sstream>

struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int sendSubType;	// Sending sub-type
	char buffer[128];
	int len;
	clock_t responseTime;
	clock_t currentTime;
};

const int TIME_PORT = 27015;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;

// Define constants for different HTTP methods
const int OPTIONS = 1;
const int GET = 2;
const int HEAD = 3;
const int POST = 4;
const int PUT = 5;
const int DEL = 6;
const int TRACE = 7;

bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void RemoveReadCharacters(int index, int numOfChars);
void sendMessage(int index);

struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;


void main()
{
	// Initialize Winsock (Windows Sockets).

	// Create a WSADATA object called wsaData.
	// The WSADATA structure contains information about the Windows 
	// Sockets implementation.
	WSAData wsaData;

	// Call WSAStartup and return its value as an integer and check for errors.
	// The WSAStartup function initiates the use of WS2_32.DLL by a process.
	// First parameter is the version number 2.2.
	// The WSACleanup function destructs the use of WS2_32.DLL by a process.
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Server: Error at WSAStartup()\n";
		return;
	}

	// Server side:
	// Create and bind a socket to an internet address.
	// Listen through the socket for incoming connections.

	// After initialization, a SOCKET object is ready to be instantiated.

	// Create a SOCKET object called listenSocket. 
	// For this application:	use the Internet address family (AF_INET), 
	//							streaming sockets (SOCK_STREAM), 
	//							and the TCP/IP protocol (IPPROTO_TCP).
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	// Error detection is a key part of successful networking code. 
	// If the socket call fails, it returns INVALID_SOCKET. 
	// The if statement in the previous code is used to catch any errors that
	// may have occurred while creating the socket. WSAGetLastError returns an 
	// error number associated with the last error that occurred.
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	// For a server to communicate on a network, it must bind the socket to 
	// a network address.

	// Need to assemble the required data for connection in sockaddr structure.

	// Create a sockaddr_in object called serverService. 
	sockaddr_in serverService;
	// Address family (must be AF_INET - Internet address family).
	serverService.sin_family = AF_INET;
	// IP address. The sin_addr is a union (s_addr is a unsigned long 
	// (4 bytes) data type).
	// inet_addr (Iternet address) is used to convert a string (char *) 
	// into unsigned long.
	// The IP address is INADDR_ANY to accept connections on all interfaces.
	serverService.sin_addr.s_addr = INADDR_ANY;
	// IP Port. The htons (host to network - short) function converts an
	// unsigned short from host to TCP/IP network byte order 
	// (which is big-endian).
	serverService.sin_port = htons(TIME_PORT);

	// Bind the socket for client's requests.

	// The bind function establishes a connection to a specified socket.
	// The function uses the socket handler, the sockaddr structure (which
	// defines properties of the desired connection) and the length of the
	// sockaddr structure (in bytes).
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	// Listen on the Socket for incoming connections.
	// This socket accepts only one connection (no pending connections 
	// from other clients). This sets the backlog parameter.
	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN);

	// Accept connections and handles them one by one.
	while (true)
	{
		// The select function determines the status of one or more sockets,
		// waiting if necessary, to perform asynchronous I/O. Use fd_sets for
		// sets of handles for reading, writing and exceptions. select gets "timeout" for waiting
		// and still performing other operations (Use NULL for blocking). Finally,
		// select returns the number of descriptors which are ready for use (use FD_ISSET
		// macro to check which descriptor in each set is ready to be used).
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}

		// Wait for interesting event.
		// Note: First argument is ignored. The fourth is for exceptions.
		// And as written above the last is a timeout, hence we are blocked if nothing happens.

		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:
					receiveMessage(i);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:
					sendMessage(i);
					break;
				}
			}
		}
	}

	// Closing connections and Winsock.
	cout << "Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SOCKET id, int what)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}

void acceptConnection(int index)
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	// Set the socket to be in non-blocking mode.
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;
	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "Server: Received: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].len += bytesRecv;

		if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0)
		{
			sockets[index].send = SEND;
			sockets[index].sendSubType = OPTIONS;
			RemoveReadCharacters(index, 7);
			return;
		}
		else if (strncmp(sockets[index].buffer, "GET", 3) == 0) {
			sockets[index].send = SEND;
			sockets[index].sendSubType = GET;
			RemoveReadCharacters(index, 3);
			return;
		}
		else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0) {
			sockets[index].send = SEND;
			sockets[index].sendSubType = HEAD;
			RemoveReadCharacters(index, 4);
			return;
		}
		else if (strncmp(sockets[index].buffer, "POST", 4) == 0) {
			sockets[index].send = SEND;
			sockets[index].sendSubType = POST;
			RemoveReadCharacters(index, 4);
			return;
		}
		else if (strncmp(sockets[index].buffer, "PUT", 3) == 0) {
			sockets[index].send = SEND;
			sockets[index].sendSubType = PUT;
			RemoveReadCharacters(index, 3);
			return;
		}
		else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0) {
			sockets[index].send = SEND;
			sockets[index].sendSubType = DEL;
			RemoveReadCharacters(index, 6);
			return;
		}
		else if (strncmp(sockets[index].buffer, "TRACE", 5) == 0) {
			sockets[index].send = SEND;
			sockets[index].sendSubType = TRACE;
			RemoveReadCharacters(index, 5);
			return;
		}
		else if (strncmp(sockets[index].buffer, "Exit", 4) == 0)
		{
			closesocket(msgSocket);
			removeSocket(index);
			return;
		}
	}
}

void RemoveReadCharacters(int index, int numOfChars)
{
	memcpy(sockets[index].buffer, &sockets[index].buffer[numOfChars], sockets[index].len - numOfChars);
	sockets[index].len -= numOfChars;
}


void sendMessage(int index)
{
	int bytesSent = 0;
	char sendBuff[255];
	double responseTime;
	SOCKET msgSocket = sockets[index].id;
	sockets[index].currentTime = clock();
	responseTime = ((double)sockets[index].currentTime - (double)sockets[index].responseTime) / CLOCKS_PER_SEC;
	time_t timeOfNow;
	time(&timeOfNow);

	stringstream responseStream;
	responseStream << "HTTP/1.1 200 OK\r\n"
				   << "Date: " << ctime(&timeOfNow)
		           << "Server: TCPNonBlockingServer/1.0\r\n";
	
	if (responseTime <= 120)
	{
		if (sockets[index].sendSubType == OPTIONS)
		{
			responseStream << "Allow: OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE\r\n"
						   << "Content-Length: 0\r\n";
		}
		else if (sockets[index].sendSubType == GET)
		{

		}
		else if (sockets[index].sendSubType == HEAD)
		{

		}
		else if (sockets[index].sendSubType == POST)
		{

		}
		else if (sockets[index].sendSubType == PUT)
		{

		}
		else if (sockets[index].sendSubType == DEL)
		{

		}
		else if (sockets[index].sendSubType == TRACE)
		{

		}

		responseStream << "Content-Type: text/html\r\n"
			           << "Connection: keep-alive\r\n"
			           << "\r\n";
		strcpy(sendBuff, responseStream.str().c_str());
		sendBuff[strlen(sendBuff) - 1] = 0;

		//_itoa((int)timer, sendBuff, 10);
		bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
		if (SOCKET_ERROR == bytesSent)
		{
			cout << "Server: Error at send(): " << WSAGetLastError() << endl;
			return;
		}

		cout << "Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n";

		sockets[index].send = IDLE;
	}
	else
	{
		closesocket(msgSocket);
		removeSocket(index);
		cout << "Closing the socket, 120 seconds has passed. timeout." << endl;
		return;
	}
}