// cld101x_operations.cpp
#include "include/cld101x_operations.h"
#include "include/cld101x_manager.h"  // Include the actual headers here
#include "include/cld101x_client.h"
#include "include/logger.h"
#include <chrono>
#include <thread>
#include <cmath>

CLD101xOperations::CLD101xOperations(CLD101xManager& manager)
  : m_manager(manager)
{
  m_logger = Logger::GetInstance();
  m_logger->LogInfo("CLD101xOperations: Initialized");
}

CLD101xOperations::~CLD101xOperations() {
  m_logger->LogInfo("CLD101xOperations: Destroyed");
}

CLD101xClient* CLD101xOperations::GetClient(const std::string& clientName) {
  if (clientName.empty()) {
    // No specific client requested, get the first available one
    auto clientNames = m_manager.GetClientNames();
    if (clientNames.empty()) {
      m_logger->LogError("CLD101xOperations: No clients available");
      return nullptr;
    }
    return m_manager.GetClient(clientNames[0]);
  }
  else {
    // Get the named client
    return m_manager.GetClient(clientName);
  }
}


bool CLD101xOperations::LaserOn(const std::string& clientName) {
  // Use retry version with 3 attempts by default
  return LaserOnWithRetry(clientName, 3);
}

//
//bool CLD101xOperations::LaserOn(const std::string& clientName) {
//  m_logger->LogInfo("CLD101xOperations: Turning laser on" +
//    (clientName.empty() ? "" : " for " + clientName));
//
//  CLD101xClient* client = GetClient(clientName);
//  if (!client) {
//    return false;
//  }
//
//  return client->LaserOn();
//}

bool CLD101xOperations::LaserOff(const std::string& clientName) {
  m_logger->LogInfo("CLD101xOperations: Turning laser off" +
    (clientName.empty() ? "" : " for " + clientName));

  CLD101xClient* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  return client->LaserOff();
}

bool CLD101xOperations::TECOn(const std::string& clientName) {
  m_logger->LogInfo("CLD101xOperations: Turning TEC on" +
    (clientName.empty() ? "" : " for " + clientName));

  CLD101xClient* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  return client->TECOn();
}

bool CLD101xOperations::TECOff(const std::string& clientName) {
  m_logger->LogInfo("CLD101xOperations: Turning TEC off" +
    (clientName.empty() ? "" : " for " + clientName));

  CLD101xClient* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  return client->TECOff();
}

// Wrapper methods that use retry by default
bool CLD101xOperations::SetLaserCurrent(float current, const std::string& clientName) {
  // Use retry version with 3 attempts by default
  return SetLaserCurrentWithRetry(current, clientName, 3);
}

//bool CLD101xOperations::SetLaserCurrent(float current, const std::string& clientName) {
//  m_logger->LogInfo("CLD101xOperations: Setting laser current to " +
//    std::to_string(current) + "A" +
//    (clientName.empty() ? "" : " for " + clientName));
//
//  CLD101xClient* client = GetClient(clientName);
//  if (!client) {
//    return false;
//  }
//
//  return client->SetLaserCurrent(current);
//}

bool CLD101xOperations::SetTECTemperature(float temperature, const std::string& clientName) {
  m_logger->LogInfo("CLD101xOperations: Setting TEC temperature to " +
    std::to_string(temperature) + "C" +
    (clientName.empty() ? "" : " for " + clientName));

  CLD101xClient* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  return client->SetTECTemperature(temperature);
}

float CLD101xOperations::GetTemperature(const std::string& clientName) {
  CLD101xClient* client = GetClient(clientName);
  if (!client) {
    return 0.0f;
  }

  return client->GetTemperature();
}

float CLD101xOperations::GetLaserCurrent(const std::string& clientName) {
  CLD101xClient* client = GetClient(clientName);
  if (!client) {
    return 0.0f;
  }

  return client->GetLaserCurrent();
}

bool CLD101xOperations::IsLaserOn(const std::string& clientName) {
  // This would require additional state tracking in CLD101xClient
  // For now, we'll just return true if the current is above a threshold
  float current = GetLaserCurrent(clientName);
  return current > 0.001f; // Threshold of 1mA to determine if laser is on
}

bool CLD101xOperations::IsTECOn(const std::string& clientName) {
  // This would require additional state tracking in CLD101xClient
  // For now, we'll assume TEC is on if temperature is being controlled
  // which means it's different from ambient (assumed to be ~22-25°C)
  float temp = GetTemperature(clientName);
  return temp < 22.0f || temp > 25.0f;
}

bool CLD101xOperations::WaitForTemperatureStabilization(
  float targetTemp, float tolerance, int timeoutMs, const std::string& clientName) {

  m_logger->LogInfo("CLD101xOperations: Waiting for temperature to stabilize at " +
    std::to_string(targetTemp) + "C (±" + std::to_string(tolerance) + "C)" +
    (clientName.empty() ? "" : " for " + clientName));

  CLD101xClient* client = GetClient(clientName);
  if (!client) {
    return false;
  }

  // Track how long we've been within the tolerance band
  const int stabilityTimeNeeded = 2000; // ms - need to be stable for 2 seconds
  int stabilityTime = 0;

  auto startTime = std::chrono::steady_clock::now();
  auto endTime = startTime + std::chrono::milliseconds(timeoutMs);

  bool wasInTolerance = false;
  auto lastCheckTime = startTime;

  while (std::chrono::steady_clock::now() < endTime) {
    float currentTemp = client->GetTemperature();
    bool inTolerance = std::abs(currentTemp - targetTemp) <= tolerance;

    // Calculate time elapsed since last check
    auto now = std::chrono::steady_clock::now();
    int elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - lastCheckTime).count();
    lastCheckTime = now;

    if (inTolerance) {
      if (wasInTolerance) {
        // Still in tolerance, add to stability time
        stabilityTime += elapsedMs;

        if (stabilityTime >= stabilityTimeNeeded) {
          m_logger->LogInfo("CLD101xOperations: Temperature stabilized at " +
            std::to_string(currentTemp) + "C");
          return true;
        }
      }
      else {
        // Just entered tolerance, start tracking stability
        stabilityTime = elapsedMs;
        wasInTolerance = true;
      }
    }
    else {
      // Outside tolerance, reset stability tracking
      stabilityTime = 0;
      wasInTolerance = false;
    }

    // Log current temperature every second
    int totalElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - startTime).count();
    if (totalElapsedMs % 1000 < 100) {  // Log roughly every second
      m_logger->LogInfo("CLD101xOperations: Current temperature: " +
        std::to_string(currentTemp) + "C, target: " +
        std::to_string(targetTemp) + "C");
    }

    // Sleep to avoid tight loop
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  m_logger->LogWarning("CLD101xOperations: Timed out waiting for temperature stabilization");
  return false;
}

bool CLD101xOperations::SetLaserCurrentWithRetry(float current, const std::string& clientName, int maxRetries) {
  m_logger->LogInfo("CLD101xOperations: Setting laser current to " +
    std::to_string(current) + "A with retry" +
    (clientName.empty() ? "" : " for " + clientName));

  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    CLD101xClient* client = GetClient(clientName);
    if (!client) {
      m_logger->LogError("CLD101xOperations: No client available for laser current setting");
      return false;
    }

    // Check if client is connected
    if (!client->IsConnected()) {
      m_logger->LogWarning("CLD101xOperations: Client not connected, attempting reconnection (attempt " +
        std::to_string(attempt) + "/" + std::to_string(maxRetries) + ")");

      // Try to reconnect - you'll need to add this method to CLD101xClient
      if (!TryReconnectClient(client)) {
        if (attempt < maxRetries) {
          m_logger->LogInfo("CLD101xOperations: Reconnection failed, waiting 2 seconds before retry...");
          std::this_thread::sleep_for(std::chrono::seconds(2));
          continue;
        }
        else {
          m_logger->LogError("CLD101xOperations: All reconnection attempts failed");
          return false;
        }
      }
    }

    // Try to set the laser current
    if (client->SetLaserCurrent(current)) {
      if (attempt > 1) {
        m_logger->LogInfo("CLD101xOperations: Laser current set successfully on attempt " + std::to_string(attempt));
      }
      return true;
    }

    // If we get here, the command failed
    if (attempt < maxRetries) {
      m_logger->LogWarning("CLD101xOperations: Failed to set laser current (attempt " +
        std::to_string(attempt) + "/" + std::to_string(maxRetries) + "), retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    else {
      m_logger->LogError("CLD101xOperations: Failed to set laser current after " + std::to_string(maxRetries) + " attempts");
    }
  }

  return false;
}

bool CLD101xOperations::LaserOnWithRetry(const std::string& clientName, int maxRetries) {
  m_logger->LogInfo("CLD101xOperations: Turning laser on with retry" +
    (clientName.empty() ? "" : " for " + clientName));

  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    CLD101xClient* client = GetClient(clientName);
    if (!client) {
      m_logger->LogError("CLD101xOperations: No client available for laser on");
      return false;
    }

    // Check connection and try to reconnect if needed
    if (!client->IsConnected()) {
      m_logger->LogWarning("CLD101xOperations: Client not connected, attempting reconnection (attempt " +
        std::to_string(attempt) + "/" + std::to_string(maxRetries) + ")");

      if (!TryReconnectClient(client)) {
        if (attempt < maxRetries) {
          m_logger->LogInfo("CLD101xOperations: Reconnection failed, waiting 2 seconds before retry...");
          std::this_thread::sleep_for(std::chrono::seconds(2));
          continue;
        }
        else {
          m_logger->LogError("CLD101xOperations: All reconnection attempts failed");
          return false;
        }
      }
    }

    // Try to turn laser on
    if (client->LaserOn()) {
      if (attempt > 1) {
        m_logger->LogInfo("CLD101xOperations: Laser turned on successfully on attempt " + std::to_string(attempt));
      }
      return true;
    }

    // If we get here, the command failed
    if (attempt < maxRetries) {
      m_logger->LogWarning("CLD101xOperations: Failed to turn laser on (attempt " +
        std::to_string(attempt) + "/" + std::to_string(maxRetries) + "), retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    else {
      m_logger->LogError("CLD101xOperations: Failed to turn laser on after " + std::to_string(maxRetries) + " attempts");
    }
  }

  return false;
}

// Helper method to try reconnecting a client
bool CLD101xOperations::TryReconnectClient(CLD101xClient* client) {
  if (!client) return false;

  // Get the manager to try reconnecting this client
  // You might need to store connection info in the client or manager
  // For now, let's assume the manager can handle reconnection

  // If your CLD101xManager has a reconnect method:
  auto clientNames = m_manager.GetClientNames();
  for (const auto& name : clientNames) {
    if (m_manager.GetClient(name) == client) {
      // Try to reconnect this specific client
      // You'll need to implement this in your manager
      return m_manager.ReconnectClient(name);
    }
  }

  return false;
}