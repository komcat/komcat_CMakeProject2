// ============================================================================
// UI SERVICES ARCHITECTURE - Clean, Dependency-Free Design
// ============================================================================

// Core principle: UI components register themselves as services
// Main UI just calls services - no direct dependencies!

// src/ui/services/UIServiceRegistry.h
#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>
#include <algorithm>
#include <set>

// Base interface for all UI services
class IUIService {
public:
  virtual ~IUIService() = default;
  virtual void RenderUI() = 0;
  virtual std::string GetServiceName() const = 0;
  virtual std::string GetDisplayName() const = 0;
  virtual std::string GetCategory() const = 0;
  virtual bool IsAvailable() const = 0;
};

// Service info for menu building
struct UIServiceInfo {
  std::string serviceName;
  std::string displayName;
  std::string category;
  std::string icon;
  bool available;
  int priority = 100; // Lower = higher priority in menus
};

// Central registry - no dependencies!
class UIServiceRegistry {
public:
  static UIServiceRegistry& Instance() {
    static UIServiceRegistry instance;
    return instance;
  }

  // Register a UI service
  void RegisterService(std::shared_ptr<IUIService> service) {
    if (!service) return;

    services[service->GetServiceName()] = service;

    UIServiceInfo info;
    info.serviceName = service->GetServiceName();
    info.displayName = service->GetDisplayName();
    info.category = service->GetCategory();
    info.available = service->IsAvailable();
    // You could add icons based on service name
    info.icon = GetIconForService(service->GetServiceName());

    serviceInfos[service->GetServiceName()] = info;
  }

  // Get service by name
  std::shared_ptr<IUIService> GetService(const std::string& name) {
    auto it = services.find(name);
    return (it != services.end()) ? it->second : nullptr;
  }

  // Get all services in a category
  std::vector<UIServiceInfo> GetServicesByCategory(const std::string& category) {
    std::vector<UIServiceInfo> result;
    for (const auto& [name, info] : serviceInfos) {
      if (info.category == category) {
        result.push_back(info);
      }
    }
    // Sort by priority
    std::sort(result.begin(), result.end(),
      [](const UIServiceInfo& a, const UIServiceInfo& b) {
      return a.priority < b.priority;
    });
    return result;
  }

  // Get all categories
  std::vector<std::string> GetAllCategories() {
    std::set<std::string> categories;
    for (const auto& [name, info] : serviceInfos) {
      categories.insert(info.category);
    }
    return std::vector<std::string>(categories.begin(), categories.end());
  }

private:
  std::unordered_map<std::string, std::shared_ptr<IUIService>> services;
  std::unordered_map<std::string, UIServiceInfo> serviceInfos;

  std::string GetIconForService(const std::string& serviceName) {
    // Simple icon mapping
    if (serviceName.find("PI") != std::string::npos) return "🤖";
    if (serviceName.find("Gantry") != std::string::npos) return "🦾";
    if (serviceName.find("IO") != std::string::npos) return "⚡";
    if (serviceName.find("Camera") != std::string::npos) return "📷";
    if (serviceName.find("Vision") != std::string::npos) return "👁";
    if (serviceName.find("Data") != std::string::npos) return "📊";
    return "🔧";
  }
};

// Convenience macro for service registration
#define REGISTER_UI_SERVICE(serviceClass, ...) \
    UIServiceRegistry::Instance().RegisterService(std::make_shared<serviceClass>(__VA_ARGS__));