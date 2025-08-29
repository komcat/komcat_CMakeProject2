#pragma once

// === WINSOCK CONFLICT PREVENTION ===
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include <algorithm>
class TcpClient {
public:
    TcpClient();
    ~TcpClient();

    // Connect to server
    bool Connect(const std::string& ip, int port);

    // Disconnect from server
    void Disconnect();

    // Check if connected
    bool IsConnected() const;

    // Get the latest received float value
    float GetLatestValue();

    // Get all received values since last call (clears the queue)
    std::deque<float> GetReceivedValues();

private:
    // Receive data thread function
    void ReceiveThread();

    SOCKET m_socket;
    std::string m_serverIp;
    int m_serverPort;

    std::thread m_receiveThread;
    std::atomic<bool> m_isRunning;
    std::atomic<bool> m_isConnected;

    std::deque<float> m_receivedValues;
    std::mutex m_valueMutex;
    float m_latestValue;
};