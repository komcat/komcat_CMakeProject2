#pragma once

#include <string>
#include <cstdint>

/**
 * @brief Cross-platform socket wrapper to avoid winsock conflicts
 * Hides all socket implementation details from headers
 */
class SocketWrapper {
public:
  SocketWrapper();
  ~SocketWrapper();

  // Disable copy/move to avoid socket handle issues
  SocketWrapper(const SocketWrapper&) = delete;
  SocketWrapper& operator=(const SocketWrapper&) = delete;
  SocketWrapper(SocketWrapper&&) = delete;
  SocketWrapper& operator=(SocketWrapper&&) = delete;

  // Connection management
  bool Connect(const std::string& host, int port);
  void Disconnect();
  bool IsConnected() const;

  // Data operations
  int Receive(char* buffer, int bufferSize);
  int Send(const char* data, int dataSize);

  // Socket configuration
  bool SetTimeout(int timeoutMs);

  // Error handling
  std::string GetLastError() const;

private:
  class Impl;
  Impl* m_pImpl;
};