// cld101x_operations.h
#pragma once

#include <string>

// Forward declarations
class CLD101xManager;
class CLD101xClient;
class Logger;

class CLD101xOperations {
public:
  CLD101xOperations(CLD101xManager& manager);
  ~CLD101xOperations();

  // Methods remain the same...
  bool LaserOn(const std::string& clientName = "");
  bool LaserOff(const std::string& clientName = "");
  bool TECOn(const std::string& clientName = "");
  bool TECOff(const std::string& clientName = "");

  bool SetLaserCurrent(float current, const std::string& clientName = "");
  bool SetTECTemperature(float temperature, const std::string& clientName = "");

  float GetTemperature(const std::string& clientName = "");
  float GetLaserCurrent(const std::string& clientName = "");
  bool IsLaserOn(const std::string& clientName = "");
  bool IsTECOn(const std::string& clientName = "");

  bool WaitForTemperatureStabilization(float targetTemp, float tolerance = 0.5f,
    int timeoutMs = 30000, const std::string& clientName = "");
  // Enhanced methods with retry capability
  bool SetLaserCurrentWithRetry(float current, const std::string& clientName = "", int maxRetries = 3);
  bool LaserOnWithRetry(const std::string& clientName = "", int maxRetries = 3);
  bool TryReconnectClient(CLD101xClient* client);

private:
  CLD101xManager& m_manager;
  Logger* m_logger;

  // Helper method to get a client, either by name or default
  CLD101xClient* GetClient(const std::string& clientName);
};