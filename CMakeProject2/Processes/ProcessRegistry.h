// ============================================================================
// ProcessRegistry.h - Dynamic Process Registration System
// ============================================================================
#pragma once

#include "SequenceStep.h"
#include "machine_operations.h"
#include "Programming/UserPromptUI.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <map>

// NEW: Process builder function signature with parameters
using ProcessBuilderWithParams = std::function<std::unique_ptr<SequenceStep>(
  MachineOperations&,
  UserPromptUI&,
  const std::map<std::string, std::string>&
)>;

// LEGACY: Keep old signature for backward compatibility
using ProcessBuilder = std::function<std::unique_ptr<SequenceStep>(MachineOperations&, UserPromptUI&)>;

// Process metadata
struct ProcessInfo {
  std::string name;
  std::string category;
  std::string description;
  bool requiresUserPromptUI;
  bool supportsParameters;  // NEW: Track if process accepts parameters

  ProcessBuilderWithParams builderWithParams;  // NEW: Enhanced builder
  ProcessBuilder legacyBuilder;                // Keep old builder

  ProcessInfo() = default;

  // NEW: Constructor for parameterized processes
  ProcessInfo(const std::string& n, const std::string& c, const std::string& d,
    bool req, ProcessBuilderWithParams b)
    : name(n), category(c), description(d), requiresUserPromptUI(req),
    supportsParameters(true), builderWithParams(b), legacyBuilder(nullptr) {
  }

  // LEGACY: Constructor for old-style processes
  ProcessInfo(const std::string& n, const std::string& c, const std::string& d,
    bool req, ProcessBuilder b)
    : name(n), category(c), description(d), requiresUserPromptUI(req),
    supportsParameters(false), builderWithParams(nullptr), legacyBuilder(b) {
  }
};

// Central registry for all processes
class ProcessRegistry {
public:
  static ProcessRegistry& GetInstance() {
    static ProcessRegistry instance;
    return instance;
  }

  // NEW: Register a parameterized process
  void RegisterProcessWithParams(const std::string& name,
    const std::string& category,
    const std::string& description,
    bool requiresUserPromptUI,
    ProcessBuilderWithParams builder) {
    m_processes[name] = ProcessInfo(name, category, description, requiresUserPromptUI, builder);
  }

  // LEGACY: Register old-style process (backward compatible)
  void RegisterProcess(const std::string& name,
    const std::string& category,
    const std::string& description,
    bool requiresUserPromptUI,
    ProcessBuilder builder) {
    m_processes[name] = ProcessInfo(name, category, description, requiresUserPromptUI, builder);
  }

  // NEW: Build process with parameters (recipe execution)
  std::unique_ptr<SequenceStep> BuildProcessWithParameters(
    const std::string& name,
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
    const std::map<std::string, std::string>& parameters) const {

    auto it = m_processes.find(name);
    if (it == m_processes.end()) {
      return nullptr;
    }

    const ProcessInfo& info = it->second;

    if (info.supportsParameters && info.builderWithParams) {
      // Use parameterized builder
      return info.builderWithParams(machineOps, promptUI, parameters);
    }
    else if (info.legacyBuilder) {
      // Fall back to legacy builder (ignoring parameters)
      return info.legacyBuilder(machineOps, promptUI);
    }

    return nullptr;
  }

  // LEGACY: Build process without parameters (backward compatible)
  std::unique_ptr<SequenceStep> BuildProcess(const std::string& name,
    MachineOperations& machineOps,
    UserPromptUI& promptUI) const {

    std::map<std::string, std::string> emptyParams;
    return BuildProcessWithParameters(name, machineOps, promptUI, emptyParams);
  }

  // Get all registered processes
  std::vector<std::string> GetAllProcessNames() const {
    std::vector<std::string> names;
    names.reserve(m_processes.size());
    for (const auto& pair : m_processes) {
      names.push_back(pair.first);
    }
    return names;
  }

  // Get processes by category
  std::vector<std::string> GetProcessesByCategory(const std::string& category) const {
    std::vector<std::string> names;
    for (const auto& pair : m_processes) {
      if (pair.second.category == category) {
        names.push_back(pair.first);
      }
    }
    return names;
  }

  // Get all available categories
  std::vector<std::string> GetAllCategories() const {
    std::vector<std::string> categories;
    for (const auto& pair : m_processes) {
      if (std::find(categories.begin(), categories.end(), pair.second.category) == categories.end()) {
        categories.push_back(pair.second.category);
      }
    }
    return categories;
  }

  // Check if process exists
  bool HasProcess(const std::string& name) const {
    return m_processes.find(name) != m_processes.end();
  }

  // Get process info
  const ProcessInfo* GetProcessInfo(const std::string& name) const {
    auto it = m_processes.find(name);
    return (it != m_processes.end()) ? &it->second : nullptr;
  }

  // Get count of registered processes
  size_t GetProcessCount() const {
    return m_processes.size();
  }

  // Debug: Print all registered processes
  void PrintAllProcesses() const {
    printf("=== Process Registry ===\n");
    printf("Total processes: %zu\n", m_processes.size());
    for (const auto& pair : m_processes) {
      printf("- %s [%s]%s: %s\n",
        pair.second.name.c_str(),
        pair.second.category.c_str(),
        pair.second.supportsParameters ? " [Parameterized]" : "",
        pair.second.description.c_str());
    }
    printf("========================\n");
  }

private:
  std::unordered_map<std::string, ProcessInfo> m_processes;

  // Prevent copying
  ProcessRegistry() = default;
  ProcessRegistry(const ProcessRegistry&) = delete;
  ProcessRegistry& operator=(const ProcessRegistry&) = delete;
};