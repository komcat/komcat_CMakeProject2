#include "SocketWrapper.h"
#include <sstream>

// Platform-specific includes isolated here
#ifdef _WIN32
    // Force winsock2 before any other Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Completely block old winsock
#define _WINSOCKAPI_
#define _INC_WINSOCK

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

typedef SOCKET socket_t;
typedef int socklen_t;

#define CLOSE_SOCKET closesocket
#define GET_SOCKET_ERROR() WSAGetLastError()
#define SOCKET_WOULD_BLOCK WSAEWOULDBLOCK
#define SOCKET_TIMEOUT WSAETIMEDOUT
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

typedef int socket_t;
const socket_t INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;

#define CLOSE_SOCKET close
#define GET_SOCKET_ERROR() errno
#define SOCKET_WOULD_BLOCK EWOULDBLOCK
#define SOCKET_TIMEOUT EAGAIN
#endif

// Pimpl implementation to hide platform details
class SocketWrapper::Impl {
public:
  socket_t socket;
  bool connected;
  std::string lastError;
  bool initialized;

  Impl() : socket(INVALID_SOCKET), connected(false), initialized(false) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
      initialized = true;
    }
    else {
      lastError = "Failed to initialize Winsock";
    }
#else
    initialized = true;
#endif
  }

  ~Impl() {
    if (socket != INVALID_SOCKET) {
      CLOSE_SOCKET(socket);
    }
#ifdef _WIN32
    if (initialized) {
      WSACleanup();
    }
#endif
  }

  void SetLastError(const std::string& operation) {
    std::stringstream ss;
#ifdef _WIN32
    int error = GET_SOCKET_ERROR();
    ss << operation << " failed with error: " << error;
#else
    ss << operation << " failed: " << strerror(errno);
#endif
    lastError = ss.str();
  }
};

SocketWrapper::SocketWrapper() : m_pImpl(new Impl()) {
}

SocketWrapper::~SocketWrapper() {
  delete m_pImpl;
}

bool SocketWrapper::Connect(const std::string& host, int port) {
  if (!m_pImpl->initialized) {
    return false;
  }

  if (m_pImpl->connected) {
    return true;
  }

  // Create socket
  m_pImpl->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_pImpl->socket == INVALID_SOCKET) {
    m_pImpl->SetLastError("socket creation");
    return false;
  }

  // Setup address
  sockaddr_in serverAddr = {};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(static_cast<uint16_t>(port));

#ifdef _WIN32
  if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) != 1) {
#else
  if (inet_aton(host.c_str(), &serverAddr.sin_addr) == 0) {
#endif
    m_pImpl->SetLastError("address conversion");
    CLOSE_SOCKET(m_pImpl->socket);
    m_pImpl->socket = INVALID_SOCKET;
    return false;
  }

  // Connect
  if (connect(m_pImpl->socket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
    m_pImpl->SetLastError("connect");
    CLOSE_SOCKET(m_pImpl->socket);
    m_pImpl->socket = INVALID_SOCKET;
    return false;
  }

  m_pImpl->connected = true;
  return true;
  }

void SocketWrapper::Disconnect() {
  if (m_pImpl->socket != INVALID_SOCKET) {
    CLOSE_SOCKET(m_pImpl->socket);
    m_pImpl->socket = INVALID_SOCKET;
  }
  m_pImpl->connected = false;
}

bool SocketWrapper::IsConnected() const {
  return m_pImpl->connected;
}

int SocketWrapper::Receive(char* buffer, int bufferSize) {
  if (!m_pImpl->connected || m_pImpl->socket == INVALID_SOCKET) {
    return -1;
  }

  int result = recv(m_pImpl->socket, buffer, bufferSize, 0);

  if (result == SOCKET_ERROR) {
    int error = GET_SOCKET_ERROR();
    if (error == SOCKET_WOULD_BLOCK || error == SOCKET_TIMEOUT) {
      return 0; // Timeout, not an error
    }
    m_pImpl->SetLastError("recv");
    m_pImpl->connected = false;
    return -1;
  }

  if (result == 0) {
    // Graceful disconnect
    m_pImpl->connected = false;
    return -1;
  }

  return result;
}

int SocketWrapper::Send(const char* data, int dataSize) {
  if (!m_pImpl->connected || m_pImpl->socket == INVALID_SOCKET) {
    return -1;
  }

  int result = send(m_pImpl->socket, data, dataSize, 0);

  if (result == SOCKET_ERROR) {
    m_pImpl->SetLastError("send");
    m_pImpl->connected = false;
    return -1;
  }

  return result;
}

bool SocketWrapper::SetTimeout(int timeoutMs) {
  if (m_pImpl->socket == INVALID_SOCKET) {
    return false;
  }

#ifdef _WIN32
  DWORD timeout = static_cast<DWORD>(timeoutMs);
  return setsockopt(m_pImpl->socket, SOL_SOCKET, SO_RCVTIMEO,
    reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0;
#else
  struct timeval tv;
  tv.tv_sec = timeoutMs / 1000;
  tv.tv_usec = (timeoutMs % 1000) * 1000;
  return setsockopt(m_pImpl->socket, SOL_SOCKET, SO_RCVTIMEO,
    reinterpret_cast<const char*>(&tv), sizeof(tv)) == 0;
#endif
}

std::string SocketWrapper::GetLastError() const {
  return m_pImpl->lastError;
}