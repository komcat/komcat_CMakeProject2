#pragma once

#include "SequenceStep.h"
#include "machine_operations.h"
#include <memory>
#include <functional>
#include <string>

// Forward declaration for user interaction
class UserInteractionManager;

// Additional operation types needed for our processes

// Extend slide operation
class ExtendSlideOperation : public SequenceOperation {
public:
  ExtendSlideOperation(const std::string& slideName)
    : m_slideName(slideName) {
  }

  bool Execute(MachineOperations& ops) override {
    return ops.ExtendSlide(m_slideName, true);
  }

  std::string GetDescription() const override {
    return "Extend pneumatic slide " + m_slideName;
  }

private:
  std::string m_slideName;
};

// Wait operation
class WaitOperation : public SequenceOperation {
public:
  WaitOperation(int milliseconds)
    : m_milliseconds(milliseconds) {
  }

  bool Execute(MachineOperations& ops) override {
    ops.Wait(m_milliseconds);
    return true;
  }

  std::string GetDescription() const override {
    return "Wait for " + std::to_string(m_milliseconds) + " ms";
  }

private:
  int m_milliseconds;
};

// User interaction manager (declaration)
class UserInteractionManager {
public:
  UserInteractionManager() = default;
  virtual ~UserInteractionManager() = default;

  // Wait for user confirmation with a message
  virtual bool WaitForConfirmation(const std::string& message) = 0;
};

// User confirmation operation
class UserConfirmOperation : public SequenceOperation {
public:
  UserConfirmOperation(const std::string& message, UserInteractionManager& uiManager)
    : m_message(message), m_uiManager(uiManager) {
  }

  bool Execute(MachineOperations& ops) override {
    return m_uiManager.WaitForConfirmation(m_message);
  }

  std::string GetDescription() const override {
    return "Wait for user confirmation: " + m_message;
  }

private:
  std::string m_message;
  UserInteractionManager& m_uiManager;
};

// Process builder namespace - contains factory functions to build sequence steps for specific processes
namespace ProcessBuilders {
  // Build an initialization sequence
  std::unique_ptr<SequenceStep> BuildInitializationSequence(MachineOperations& machineOps);

  // Build a probing sequence
  std::unique_ptr<SequenceStep> BuildProbingSequence(
    MachineOperations& machineOps, UserInteractionManager& uiManager);

  // Build a pick and place left lens sequence
  std::unique_ptr<SequenceStep> BuildPickPlaceLeftLensSequence(
    MachineOperations& machineOps, UserInteractionManager& uiManager);

  // Build a pick and place right lens sequence
  std::unique_ptr<SequenceStep> BuildPickPlaceRightLensSequence(
    MachineOperations& machineOps, UserInteractionManager& uiManager);

  // Build a UV curing sequence
  std::unique_ptr<SequenceStep> BuildUVCuringSequence(MachineOperations& machineOps, UserInteractionManager& uiManager);

  // Build a complete process sequence (combines all steps)
  std::unique_ptr<SequenceStep> BuildCompleteProcessSequence(
    MachineOperations& machineOps, UserInteractionManager& uiManager);
}