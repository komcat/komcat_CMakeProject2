// ============================================================================
// SERVICE LOCATOR PATTERN - Zero Dependencies, Maximum Flexibility
// ============================================================================

// src/core/ServiceLocator.h
#pragma once
#include <memory>
#include <iostream>

// Forward declarations for all possible services
class PIControllerManager;
class ACSControllerManager;
class CameraManager;
class EziIOManager;
class CLD101xManager;
class Keithley2400Manager;
class PneumaticManager;
class MotionConfigManager;
class MachineOperations;

class ServiceLocator {
public:
  // Singleton access
  static ServiceLocator& Instance() {
    static ServiceLocator instance;
    return instance;
  }

  // Convenience accessor
  static ServiceLocator& Get() { return Instance(); }

  // ========================================================================
  // SERVICE REGISTRATION (called once during startup)
  // ========================================================================

  void RegisterPI(PIControllerManager* service) {
    piManager = service;
    if (service) std::cout << "✅ PI Service registered" << std::endl;
  }

  void RegisterACS(ACSControllerManager* service) {
    acsManager = service;
    if (service) std::cout << "✅ ACS Service registered" << std::endl;
  }

  void RegisterCamera(CameraManager* service) {
    cameraManager = service;
    if (service) std::cout << "✅ Camera Service registered" << std::endl;
  }

  void RegisterIO(EziIOManager* service) {
    ioManager = service;
    if (service) std::cout << "✅ IO Service registered" << std::endl;
  }

  void RegisterCLD101x(CLD101xManager* service) {
    cldManager = service;
    if (service) std::cout << "✅ CLD101x Service registered" << std::endl;
  }

  void RegisterSMU(Keithley2400Manager* service) {
    smuManager = service;
    if (service) std::cout << "✅ SMU Service registered" << std::endl;
  }

  void RegisterPneumatic(PneumaticManager* service) {
    pneumaticManager = service;
    if (service) std::cout << "✅ Pneumatic Service registered" << std::endl;
  }

  void RegisterMotionConfig(MotionConfigManager* service) {
    motionConfigManager = service;
    if (service) std::cout << "✅ Motion Config Service registered" << std::endl;
  }

  void RegisterMachineOps(MachineOperations* service) {
    machineOperations = service;
    if (service) std::cout << "✅ Machine Operations Service registered" << std::endl;
  }

  // ========================================================================
  // SERVICE ACCESS (used everywhere, zero parameters needed!)
  // ========================================================================

  PIControllerManager* PI() const { return piManager; }
  ACSControllerManager* ACS() const { return acsManager; }
  CameraManager* Camera() const { return cameraManager; }
  EziIOManager* IO() const { return ioManager; }
  CLD101xManager* CLD101x() const { return cldManager; }
  Keithley2400Manager* SMU() const { return smuManager; }
  PneumaticManager* Pneumatic() const { return pneumaticManager; }
  MotionConfigManager* MotionConfig() const { return motionConfigManager; }
  MachineOperations* MachineOps() const { return machineOperations; }

  // ========================================================================
  // AVAILABILITY CHECKS (for UI availability logic)
  // ========================================================================

  bool HasPI() const { return piManager != nullptr; }
  bool HasACS() const { return acsManager != nullptr; }
  bool HasCamera() const { return cameraManager != nullptr; }
  bool HasIO() const { return ioManager != nullptr; }
  bool HasCLD101x() const { return cldManager != nullptr; }
  bool HasSMU() const { return smuManager != nullptr; }
  bool HasPneumatic() const { return pneumaticManager != nullptr; }
  bool HasMotionConfig() const { return motionConfigManager != nullptr; }
  bool HasMachineOps() const { return machineOperations != nullptr; }

  // ========================================================================
  // UTILITY METHODS
  // ========================================================================

  // Clear all services (for cleanup)
  void ClearAll() {
    piManager = nullptr;
    acsManager = nullptr;
    cameraManager = nullptr;
    ioManager = nullptr;
    cldManager = nullptr;
    smuManager = nullptr;
    pneumaticManager = nullptr;
    motionConfigManager = nullptr;
    machineOperations = nullptr;
    std::cout << "🔄 All services cleared" << std::endl;
  }

  // Get service status summary
  int GetAvailableServiceCount() const {
    int count = 0;
    if (HasPI()) count++;
    if (HasACS()) count++;
    if (HasCamera()) count++;
    if (HasIO()) count++;
    if (HasCLD101x()) count++;
    if (HasSMU()) count++;
    if (HasPneumatic()) count++;
    if (HasMotionConfig()) count++;
    if (HasMachineOps()) count++;
    return count;
  }

private:
  // Private constructor for singleton
  ServiceLocator() = default;

  // All services as simple pointers (not owned)
  PIControllerManager* piManager = nullptr;
  ACSControllerManager* acsManager = nullptr;
  CameraManager* cameraManager = nullptr;
  EziIOManager* ioManager = nullptr;
  CLD101xManager* cldManager = nullptr;
  Keithley2400Manager* smuManager = nullptr;
  PneumaticManager* pneumaticManager = nullptr;
  MotionConfigManager* motionConfigManager = nullptr;
  MachineOperations* machineOperations = nullptr;
};

// ========================================================================
// CONVENIENCE MACROS (optional, for even cleaner syntax)
// ========================================================================

#define Services ServiceLocator::Get()

// Example usage:
// auto pi = Services.PI();
// if (Services.HasCamera()) { auto cam = Services.Camera(); }

// ========================================================================
// SAFER SERVICE ACCESS WITH AUTOMATIC NULL CHECKS
// ========================================================================

// Template helper for safe service access
template<typename T>
class SafeService {
public:
  SafeService(T* service) : service_(service) {}

  // Implicit bool conversion
  operator bool() const { return service_ != nullptr; }

  // Arrow operator for direct access
  T* operator->() const { return service_; }

  // Get raw pointer
  T* get() const { return service_; }

  // Execute if available
  template<typename Func>
  void IfAvailable(Func&& func) {
    if (service_) {
      func(service_);
    }
  }

private:
  T* service_;
};

// Safe accessors
namespace SafeServices {
  inline SafeService<PIControllerManager> PI() {
    return SafeService<PIControllerManager>(Services.PI());
  }

  inline SafeService<CameraManager> Camera() {
    return SafeService<CameraManager>(Services.Camera());
  }

  inline SafeService<EziIOManager> IO() {
    return SafeService<EziIOManager>(Services.IO());
  }

  // Add more as needed...
}

// Usage examples:
/*
// Method 1: Simple access with manual null check
auto pi = Services.PI();
if (pi) {
    pi->MoveAxis(0, 10.0);
}

// Method 2: Safe service with automatic checking
auto camera = SafeServices::Camera();
if (camera) {
    camera->CaptureImage();
}

// Method 3: Execute if available
SafeServices::PI().IfAvailable([](auto* pi) {
    pi->HomeAllAxes();
});
*/