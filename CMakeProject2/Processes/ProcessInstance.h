#pragma once
#include <string>
#include <map>

// Structure to hold process instance data
struct ProcessInstance {
  std::string instanceId;      // e.g., "PFEC02007"
  std::string processType;     // e.g., "Core_PickPlace"
  std::string displayName;     // e.g., "CorePickPlace_PFEC02007"
  std::string nickname;        // NEW: User-friendly name

  // Parameters for the process
  std::map<std::string, std::string> parameters;

  ProcessInstance(const std::string& id, const std::string& type)
    : instanceId(id), processType(type), nickname("") {
    displayName = type + "_" + id;
  }

  // Get the name to show in UI - nickname if set, otherwise displayName
  std::string GetUIDisplayName() const {
    return nickname.empty() ? displayName : nickname;
  }
};