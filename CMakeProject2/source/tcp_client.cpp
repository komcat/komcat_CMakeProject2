#include "include/tcp_client.h"
#include <iostream>
#include <chrono>
#include <algorithm> // Add this line for std::min and std::max

TcpClient::TcpClient()
    : m_socket(INVALID_SOCKET)
    , m_serverPort(0)
    , m_isRunning(false)
    , m_isConnected(false)
    , m_latestValue(0.0f)
{
#ifdef _WIN32
    // Initialize Winsock for Windows
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
    }
#endif
}

TcpClient::~TcpClient() {
    Disconnect();

#ifdef _WIN32
    // Cleanup Winsock for Windows
    WSACleanup();
#endif
}

bool TcpClient::Connect(const std::string& ip, int port) {
    // If already connected, disconnect first
    if (m_isConnected) {
        Disconnect();
    }

    m_serverIp = ip;
    m_serverPort = port;

    // Create socket
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "Error creating socket" << std::endl;
        return false;
    }

    // Setup server address
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    // Convert IP address from string to binary form
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << ip << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    // Connect to server
    if (connect(m_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed" << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    // Set socket to non-blocking mode
#ifdef _WIN32
    unsigned long mode = 1;
    if (ioctlsocket(m_socket, FIONBIO, &mode) != 0) {
        std::cerr << "Failed to set non-blocking mode" << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }
#else
    int flags = fcntl(m_socket, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "Failed to get socket flags" << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    if (fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "Failed to set non-blocking mode" << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }
#endif

    // Start receive thread
    m_isRunning = true;
    m_isConnected = true;
    m_receiveThread = std::thread(&TcpClient::ReceiveThread, this);

    return true;
}

void TcpClient::Disconnect() {
    // Stop receive thread
    m_isRunning = false;
    m_isConnected = false;

    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }

    // Close socket
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

bool TcpClient::IsConnected() const {
    return m_isConnected;
}

float TcpClient::GetLatestValue() {
    return m_latestValue;
}

std::deque<float> TcpClient::GetReceivedValues() {
    std::lock_guard<std::mutex> lock(m_valueMutex);
    std::deque<float> values = m_receivedValues;
    m_receivedValues.clear();
    return values;
}

// Replace this function in tcp_client.cpp

void TcpClient::ReceiveThread() {
    const int bufferSize = 256;
    char buffer[bufferSize];
    std::string dataBuffer;  // Buffer to accumulate partial messages

    while (m_isRunning) {
        // Receive data
        int bytesReceived = recv(m_socket, buffer, bufferSize - 1, 0);

        if (bytesReceived > 0) {
            // Null-terminate the received data for string operations
            buffer[bytesReceived] = '\0';

            // Add to our string buffer
            dataBuffer.append(buffer, bytesReceived);

            // Process complete lines in the buffer
            size_t pos;
            while ((pos = dataBuffer.find('\n')) != std::string::npos) {
                // Extract a complete line
                std::string line = dataBuffer.substr(0, pos);
                dataBuffer.erase(0, pos + 1);  // Remove processed line

                try {
                    // Convert to float
                    float value = std::stof(line);

                    // Debug output
                    //std::cout << "Received value: " << value << std::endl;

                    // Store the value
                    {
                        std::lock_guard<std::mutex> lock(m_valueMutex);
                        m_receivedValues.push_back(value);
                        // Limit queue size
                        if (m_receivedValues.size() > 1000) {
                            m_receivedValues.pop_front();
                        }
                        m_latestValue = value;
                    }
                }
                catch (const std::exception& e) {
                    // Handle conversion errors (ignore malformed data)
                    std::cerr << "Error parsing float value: " << line << " - " << e.what() << std::endl;
                }
            }
        }
        else if (bytesReceived == 0) {
            // Connection closed by server
            m_isConnected = false;
            std::cerr << "Connection closed by server" << std::endl;
            break;
        }
        else if (bytesReceived == SOCKET_ERROR) {
#ifdef _WIN32
            int errorCode = WSAGetLastError();
            if (errorCode != WSAEWOULDBLOCK) {
                m_isConnected = false;
                std::cerr << "Socket error: " << errorCode << std::endl;
                break;
            }
#else
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                m_isConnected = false;
                std::cerr << "Socket error: " << errno << std::endl;
                break;
            }
#endif
        }

        // Sleep a bit to avoid high CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}