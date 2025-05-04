#pragma once

#include "ProcessStep.h"
#include <vector>
#include <string>
#include <functional>
#include <memory>

// Represents an operation in a sequence
class SequenceOperation {
public:
  virtual ~SequenceOperation() = default;
  virtual bool Execute(MachineOperations& ops) = 0;
  virtual std::string GetDescription() const = 0;
};

// Move device to node operation
class MoveToNodeOperation : public SequenceOperation {
public:
  MoveToNodeOperation(const std::string& deviceName, const std::string& graphName,
    const std::string& nodeId)
    : m_deviceName(deviceName), m_graphName(graphName), m_nodeId(nodeId) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.MoveDeviceToNode(m_deviceName, m_graphName, m_nodeId, true);
  }

  std::string GetDescription() const override {
    return "Move " + m_deviceName + " to node " + m_nodeId + " in graph " + m_graphName;
  }

private:
  std::string m_deviceName;
  std::string m_graphName;
  std::string m_nodeId;
};

// Set output operation
class SetOutputOperation : public SequenceOperation {
public:
  SetOutputOperation(const std::string& deviceName, int pin, bool state)
    : m_deviceName(deviceName), m_pin(pin), m_state(state) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.SetOutput(m_deviceName, m_pin, m_state);
  }

  std::string GetDescription() const override {
    return "Set output " + m_deviceName + " pin " + std::to_string(m_pin) +
      " to " + (m_state ? "ON" : "OFF");
  }

private:
  std::string m_deviceName;
  int m_pin;
  bool m_state;
};

// Sequence step - executes a sequence of operations
class SequenceStep : public ProcessStep {
public:
  SequenceStep(const std::string& name, MachineOperations& machineOps);

  // Add operations to the sequence
  void AddOperation(std::shared_ptr<SequenceOperation> operation);

  // Execute the sequence
  bool Execute() override;

private:
  std::vector<std::shared_ptr<SequenceOperation>> m_operations;
};