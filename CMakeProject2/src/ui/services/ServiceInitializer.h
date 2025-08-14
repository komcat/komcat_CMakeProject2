// src/ui/services/ServiceInitializer.h
// Central service registration - includes all service files
#pragma once

#include "UIServiceRegistry.h"
#include "utils/Logger.h"
#include "utils/Unicode.h"

// Include all service headers organized by category
// Manual category
#include "manual/PIControlService.h"
#include "manual/GantryService.h"
#include "manual/IOControlService.h"
#include "manual/PneumaticService.h"

// Data category
#include "data/DataMonitorService.h"
#include "data/SMUService.h"
#include "data/CLD101xService.h"

// Vision category
#include "vision/VisionService.h"
#include "vision/FiducialService.h"
#include "vision/DatumService.h"

// Config category
#include "config/ConfigService.h"
#include "config/SystemInfoService.h"

// Programming category
#include "program/ProgrammingService.h"
#include "program/MacroService.h"

// Run category
#include "run/RunProductService.h"
#include "run/ProcessSetupService.h"

class ServiceInitializer {
public:
  static void RegisterAllServices() {
    auto& registry = UIServiceRegistry::Instance();

    Logger::Info(L"🔧 Registering UI services...");

    // Manual category services
    registry.RegisterService(std::make_shared<PIControlService>());
    registry.RegisterService(std::make_shared<GantryService>());
    registry.RegisterService(std::make_shared<IOControlService>());
    registry.RegisterService(std::make_shared<PneumaticService>());

    // Data category services
    registry.RegisterService(std::make_shared<DataMonitorService>());
    registry.RegisterService(std::make_shared<SMUService>());
    registry.RegisterService(std::make_shared<CLD101xService>());

    // Vision category services
    registry.RegisterService(std::make_shared<VisionService>());
    registry.RegisterService(std::make_shared<FiducialService>());
    registry.RegisterService(std::make_shared<DatumService>());

    // Config category services
    registry.RegisterService(std::make_shared<ConfigService>());
    registry.RegisterService(std::make_shared<SystemInfoService>());

    // Programming category services
    registry.RegisterService(std::make_shared<ProgrammingService>());
    registry.RegisterService(std::make_shared<MacroService>());

    // Run category services
    registry.RegisterService(std::make_shared<RunProductService>());
    registry.RegisterService(std::make_shared<ProcessSetupService>());

    Logger::Success(L"✅ All UI services registered successfully (16 services, 6 categories)");

    // Optional: Log service count by category
    LogServiceSummary();
  }

private:
  static void LogServiceSummary() {
    auto& registry = UIServiceRegistry::Instance();
    auto categories = registry.GetAllCategories();

    Logger::Info(L"📊 Service Summary:");
    for (const auto& category : categories) {
      auto services = registry.GetServicesByCategory(category);
      int availableCount = 0;
      for (const auto& service : services) {
        if (service.available) availableCount++;
      }

      Logger::Info(L"   " + UnicodeUtils::StringToWString(category) + L": " +
        std::to_wstring(availableCount) + L"/" + std::to_wstring(services.size()) + L" available");
    }
  }
};