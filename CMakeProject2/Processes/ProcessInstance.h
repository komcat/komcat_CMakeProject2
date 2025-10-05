// ProcessInstance.h
#pragma once
#include <string>
#include <map>

struct ProcessInstance {
  std::string instanceId;      // e.g., "P0001"
  std::string processType;     // e.g., "Core_PickPlace"
  std::string displayName;     // e.g., "CorePickPlace_P0001"
  std::string nickname;        // Optional user-friendly name

  // NEW: Group support
  std::string groupId;         // e.g., "group_init" or "" for ungrouped
  int executionOrder;          // Order within group (1, 2, 3...)

  // Parameters for the process
  std::map<std::string, std::string> parameters;

  ProcessInstance(const std::string& id, const std::string& type)
    : instanceId(id),
    processType(type),
    displayName(type + "_" + id),
    groupId(""),           // NEW: Default ungrouped
    executionOrder(0) {    // NEW: Default no order
  }

  // NEW: Helper methods
  bool IsGrouped() const {
    return !groupId.empty();
  }

  std::string GetUIDisplayName() const {
    return nickname.empty() ? displayName : nickname;
  }
};