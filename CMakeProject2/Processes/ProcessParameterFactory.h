#pragma once
#include "ProcessInstance.h"  // Include the separate header
#include <functional>
#include <map>

using ParameterInitializer = std::function<void(ProcessInstance&)>;

class ProcessParameterFactory {
public:
  static void RegisterParameterInitializer(const std::string& processType,
    ParameterInitializer initializer) {
    s_initializers[processType] = initializer;
  }

  static void InitializeParameters(ProcessInstance& instance) {
    auto it = s_initializers.find(instance.processType);
    if (it != s_initializers.end()) {
      it->second(instance);
    }
  }

  static bool HasInitializer(const std::string& processType) {
    return s_initializers.find(processType) != s_initializers.end();
  }

private:
  static std::map<std::string, ParameterInitializer> s_initializers;
};
