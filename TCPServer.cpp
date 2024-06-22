#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <fstream>
#include <sstream>

struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int sendSubType;	// Sending sub-type
	char buffer[1024];
	int len;
	clock_t responseTime;
	clock_t currentTime;
};

// Our response with default values
struct responseMessage
{
	string httpVersion = "HTTP/1.1";
	string statusCode = "200 OK";
	string date = "";
	string serverName = "Server: TCPNonBlockingServer/1.0";
	string responseData = "";
	string contentLength = "";
	string contentType = "Content-Type: text/html";
	string connection = "Connection: keep-alive";
};

const int TIME_PORT = 27015;
const int MAX_SOCKETS = 2;
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
responseMessage* receiveMessage(int index);
void ProcessOptionsRequest(responseMessage* response);
void ProcessGetOrHeadRequest(responseMessage* response, char* request, bool isHead);
void ProcessPutRequest(responseMessage* response, char* request);
void RemoveReadCharacters(int index, int numOfChars);
void sendMessage(int index, responseMessage* response);
string ResponseToString(responseMessage* response, bool isHead);

struct SocketState sockets[MAX_SOCKETS] = { 0 };
responseMessage* responseArray[MAX_SOCKETS] = { nullptr };
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
		// Check for timeouts
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].recv != EMPTY)
			{
				clock_t currentTime = clock();
				double elapsedTime = ((double)currentTime - (double)sockets[i].responseTime) / CLOCKS_PER_SEC;
				if (elapsedTime > 120)
				{
					cout << "Closing socket " << i << " due to timeout." << endl;
					closesocket(sockets[i].id);
					removeSocket(i);
				}
			}
		}

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
		
		// make it with index
		responseMessage* response = nullptr;

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
					// index of response
					responseArray[i] = receiveMessage(i);
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
					responseMessage* response = responseArray[i];
					if (response != nullptr)
					{
						sendMessage(i, response);
						delete response;
						responseArray[i] = nullptr;
					}

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

responseMessage* receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;
	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		if (WSAGetLastError() == WSAEWOULDBLOCK)
		{
			return nullptr;
		}

		cout << "Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return nullptr;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return nullptr;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "Server: Received: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].len += bytesRecv;
		sockets[index].responseTime = clock();
		time_t timeOfNow;
		time(&timeOfNow);
		responseMessage* response = new responseMessage();
		response->date = "Date: " + string(ctime(&timeOfNow));

		if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0)
		{
			sockets[index].send = SEND;
			sockets[index].sendSubType = OPTIONS;
			RemoveReadCharacters(index, 7);
			ProcessOptionsRequest(response);
		}
		else if (strncmp(sockets[index].buffer, "GET", 3) == 0) {
			sockets[index].send = SEND;
			sockets[index].sendSubType = GET;
			ProcessGetOrHeadRequest(response, sockets[index].buffer, false);
		}
		else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0) {
			sockets[index].send = SEND;
			sockets[index].sendSubType = HEAD;
			ProcessGetOrHeadRequest(response, sockets[index].buffer, true);
		}
		else if (strncmp(sockets[index].buffer, "POST", 4) == 0) {
			sockets[index].send = SEND;
			sockets[index].sendSubType = POST;
			RemoveReadCharacters(index, 4);
			// IMPLEMENTATION OF POST METHOD
		}
		else if (strncmp(sockets[index].buffer, "PUT", 3) == 0) {
			sockets[index].send = SEND;
			sockets[index].sendSubType = PUT;
			ProcessPutRequest(response, sockets[index].buffer);
		}
		else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0) {
			sockets[index].send = SEND;
			sockets[index].sendSubType = DEL;
			RemoveReadCharacters(index, 6);
			// IMPLEMENTATION OF DELETE METHOD
		}
		else if (strncmp(sockets[index].buffer, "TRACE", 5) == 0) {
			sockets[index].send = SEND;
			sockets[index].sendSubType = TRACE;
			RemoveReadCharacters(index, 5);
			// IMPLEMENTATION OF TRACE METHOD
		}
		else if (strncmp(sockets[index].buffer, "Exit", 4) == 0)
		{
			closesocket(msgSocket);
			removeSocket(index);
			return nullptr;
		}

		memset(sockets[index].buffer, 0, sizeof(sockets[index].buffer));
		sockets[index].len = 0;

		return response;
	}
}

void ProcessOptionsRequest(responseMessage* response)
{
	response->responseData = "Allow: OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE";
	response->contentType = "Content-Type: text/plain";
	response->contentLength = "Content-Length: " + to_string(response->responseData.size());
}

void ProcessGetOrHeadRequest(responseMessage* response, char* request, bool isHead)
{
	string method, path, version;
	istringstream requestStream(request);
	requestStream >> method >> path >> version;
	
	string lang = "";

	// Check if language is specified in the path
	size_t langPos = path.find_last_of("_");
	if (langPos != string::npos && langPos + 3 < path.length())
	{
		lang = path.substr(langPos + 1, 2); // Extract the language code (assuming it's a two-letter code)
	}

	// Check if language is specified as a query parameter
	if (path.find("lang=") != string::npos) 
	{
		size_t queryPos = path.find('?');
		if (queryPos != string::npos) 
		{
			string query = path.substr(queryPos + 1);
			size_t langParamPos = query.find("lang=");
			if (langParamPos != string::npos && langParamPos + 5 <= query.size()) 
			{
				size_t endPos = query.find('&', langParamPos);
				lang = query.substr(langParamPos + 5, (endPos == string::npos ? query.size() : endPos) - (langParamPos + 5));
			}
		}
	}

	string fileName;
	if (!lang.empty()) 
	{
		fileName = "index_" + lang + ".html"; // Generate the file name based on the language
	}
	else 
	{
		fileName = "index_en.html"; // Default to English if no language is specified
	}

	std::ifstream file(fileName, ios::binary);
	if (file)
	{
		file.seekg(0, ios::end);
		streampos fileSize = file.tellg();
		file.seekg(0, ios::beg);

		response->contentLength = "Content-Length: " + to_string(fileSize);

		if (!isHead)  // GET requests
		{
			std::stringstream buffer;
			buffer << file.rdbuf();
			response->responseData = buffer.str();
		}

		file.close();
	}
	else 
	{
		response->statusCode = "404 Not Found";
		response->responseData = "File not found";
		response->contentLength = "Content-Length: " + to_string(response->responseData.length());
		response->connection = "Connection: close";
	}
}

void ProcessPutRequest(responseMessage* response, char* request)
{
	string method, path, version;
	istringstream requestStream(request);
	requestStream >> method >> path >> version;
	string fileName;
	bool validRequest = false;

	if (!path.empty() && path[0] == '/')
	{
		path = path.substr(1);
	}

	size_t queryPos = path.find('?');
	if (queryPos != string::npos)
	{
		fileName = path.substr(0, queryPos);
	}
	else
	{
		fileName = path;
	}

	FILE* file = fopen(fileName.c_str(), "wb");
	if (file) 
	{
		// Find the Content-Length header
		char* contentLengthPos = strstr(request, "Content-Length:");
		if (contentLengthPos != nullptr)
		{
			int contentLength;
			if (sscanf(contentLengthPos + 15, "%d", &contentLength) == 1)
			{
				validRequest = true;

				// Find the start of the data in the request
				char* dataPos = strstr(request, "\r\n\r\n");
				if (dataPos != nullptr)
				{
					dataPos += 4;

					// Write the data to the file
					size_t bytesWritten = fwrite(dataPos, sizeof(char), contentLength, file);
					if (bytesWritten != static_cast<size_t>(contentLength))
					{
						// Error while writing to the file
						response->statusCode = "500 Internal Server Error";
						response->contentLength = "Content-Length: 0";
						fclose(file);
						return;
					}
				}
			}
		}

		if (!validRequest)
		{
			response->statusCode = "400 Bad Request";
			response->contentLength = "Content-Length: 0";
		}

		fclose(file);
	}
	else 
	{
		response->statusCode = "404 Not Found";
		response->responseData = "File not found";
		response->contentLength = "Content-Length: " + to_string(response->responseData.length());
		response->connection = "Connection: close";
	}
}


void RemoveReadCharacters(int index, int numOfChars)
{
	memcpy(sockets[index].buffer, &sockets[index].buffer[numOfChars], sockets[index].len - numOfChars);
	sockets[index].len -= numOfChars;
}

void sendMessage(int index, responseMessage* response)
{
	SOCKET msgSocket = sockets[index].id;
	bool isHead = (sockets[index].sendSubType == HEAD);

	string responseStr = ResponseToString(response, isHead);
	const char* sendBuff = responseStr.c_str();
	int totalLen = responseStr.length();
	int bytesSent = 0;

	while (bytesSent < totalLen)
	{
		int result = send(msgSocket, sendBuff + bytesSent, totalLen - bytesSent, 0);
		if (result == SOCKET_ERROR)
		{
			if (WSAGetLastError() == WSAEWOULDBLOCK)
			{
				// Socket is not ready to send, try again later
				return;
			}
			else
			{
				cout << "Server: Error at send(): " << WSAGetLastError() << endl;
				closesocket(msgSocket);
				removeSocket(index);
				return;
			}
		}
		bytesSent += result;
	}

	cout << "Server: Sent: " << bytesSent << "\\" << totalLen << " bytes of response.\n";
	sockets[index].send = IDLE;
}

string ResponseToString(responseMessage* response, bool isHead)
{
	stringstream responseStream;

	responseStream << response->httpVersion << " " << response->statusCode << "\r\n"
		<< response->date
		<< response->serverName << "\r\n"
		<< response->contentLength << "\r\n"
		<< response->contentType << "\r\n"
		<< response->connection << "\r\n\r\n";

	if (!isHead)
	{
		responseStream << response->responseData << "\r\n";
	}

	return responseStream.str();
}