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

// Process builder function signature
using ProcessBuilder = std::function<std::unique_ptr<SequenceStep>(MachineOperations&, UserPromptUI&)>;

// Process metadata
struct ProcessInfo {
    std::string name;
    std::string category;
    std::string description;
    bool requiresUserPromptUI;
    ProcessBuilder builder;

    ProcessInfo() = default;
    ProcessInfo(const std::string& n, const std::string& c, const std::string& d, bool req, ProcessBuilder b)
        : name(n), category(c), description(d), requiresUserPromptUI(req), builder(b) {
    }
};

// Central registry for all processes
class ProcessRegistry {
public:
    static ProcessRegistry& GetInstance() {
        static ProcessRegistry instance;
        return instance;
    }

    // Register a process
    void RegisterProcess(const std::string& name,
        const std::string& category,
        const std::string& description,
        bool requiresUserPromptUI,
        ProcessBuilder builder) {
        m_processes[name] = ProcessInfo(name, category, description, requiresUserPromptUI, builder);
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

    // Build a process by name
    std::unique_ptr<SequenceStep> BuildProcess(const std::string& name,
        MachineOperations& machineOps,
        UserPromptUI& promptUI) const {
        auto it = m_processes.find(name);
        if (it != m_processes.end()) {
            return it->second.builder(machineOps, promptUI);
        }
        return nullptr;
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
            printf("- %s [%s]: %s\n",
                pair.second.name.c_str(),
                pair.second.category.c_str(),
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