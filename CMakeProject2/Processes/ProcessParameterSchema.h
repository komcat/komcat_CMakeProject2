#pragma once
#include <string>
#include <vector>
#include <map>

enum class ParameterType {
  STRING,
  DOUBLE,
  BOOLEAN,
  NODE_SELECTION,    // For motion nodes
  DEVICE_SELECTION   // For device names
};

struct ParameterDefinition {
  std::string name;
  ParameterType type;
  std::string defaultValue;
  std::string description;
  std::vector<std::string> options; // For dropdowns/selections

  ParameterDefinition(const std::string& n, ParameterType t, const std::string& def, const std::string& desc)
    : name(n), type(t), defaultValue(def), description(desc) {
  }

  ParameterDefinition(const std::string& n, ParameterType t, const std::string& def, const std::string& desc,
    const std::vector<std::string>& opts)
    : name(n), type(t), defaultValue(def), description(desc), options(opts) {
  }
};

class ProcessParameterSchema {
public:
  static std::vector<ParameterDefinition> GetParametersForProcess(const std::string& processType) {
    auto it = s_schemas.find(processType);
    return (it != s_schemas.end()) ? it->second : std::vector<ParameterDefinition>{};
  }

  static void RegisterProcessSchema(const std::string& processType,
    const std::vector<ParameterDefinition>& parameters) {
    s_schemas[processType] = parameters;
  }

  static bool HasSchema(const std::string& processType) {
    return s_schemas.find(processType) != s_schemas.end();
  }

private:
  static std::map<std::string, std::vector<ParameterDefinition>> s_schemas;
};

