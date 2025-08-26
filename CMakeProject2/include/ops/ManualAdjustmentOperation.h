// include/Operations/ManualAdjustmentOperation.h
#ifndef MANUAL_ADJUSTMENT_OPERATION_H
#define MANUAL_ADJUSTMENT_OPERATION_H

#include "SequenceStep.h"
#include "machine_operations.h"
#include "Programming/UserPromptUI.h"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>

class ManualAdjustmentOperation : public SequenceOperation {
public:
  // Constructor with basic parameters
  ManualAdjustmentOperation(
    const std::string& axisSystem,
    const std::string& title,
    const std::string& instructions,
    UserPromptUI& promptUI,
    bool enableX = true,
    bool enableY = true,
    bool enableZ = true
  );

  // Builder pattern methods for optional features
  ManualAdjustmentOperation& WithChecklist(const std::vector<std::string>& items);
  ManualAdjustmentOperation& WithStepSize(double size);
  ManualAdjustmentOperation& WithShowPosition(bool show);
  ManualAdjustmentOperation& WithTimeout(int seconds);
  ManualAdjustmentOperation& WithHighlightAxes(bool highlight);
  ManualAdjustmentOperation& WithSafetyLimits(double minZ, double maxZ);

  // Main execution - follows SequenceOperation pattern
  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;

  // Static factory methods for common scenarios
  static std::shared_ptr<ManualAdjustmentOperation> CreateForNeedleTouch(
    const std::string& axisSystem, UserPromptUI& promptUI);
  static std::shared_ptr<ManualAdjustmentOperation> CreateForCameraAlignment(
    const std::string& axisSystem, UserPromptUI& promptUI);
  static std::shared_ptr<ManualAdjustmentOperation> CreateForDispenseHeight(
    const std::string& axisSystem, UserPromptUI& promptUI);

  // ImGui render method - needs to be called from main UI loop
  void RenderImGui();
  bool IsActive() const { return m_isActive.load(); }
  void SetActiveFlag(bool active) { m_isActive = active; }

private:
  // Core parameters
  std::string m_axisSystem;
  std::string m_title;
  std::string m_instructions;
  UserPromptUI& m_promptUI;
  bool m_enableX;
  bool m_enableY;
  bool m_enableZ;
  MachineOperations* m_machineOps = nullptr;  // Add this
  // Optional parameters
  std::vector<std::string> m_checklistItems;
  double m_stepSize;
  bool m_showPosition;
  int m_timeoutSeconds;
  bool m_highlightAxes;
  bool m_hasSafetyLimits;
  double m_minZ;
  double m_maxZ;

  // Internal state
  std::atomic<bool> m_adjustmentComplete;
  std::atomic<bool> m_isActive;
  std::string m_completionReason;
  std::chrono::steady_clock::time_point m_startTime;
  std::vector<bool> m_checklistStates;
  bool m_showWindow;

  // Position tracking
  double m_currentX, m_currentY, m_currentZ;
  double m_startX, m_startY, m_startZ;
  mutable std::mutex m_positionMutex;

  // ImGui state
  bool m_cancelConfirmationOpen;

  // Helper methods
  void EnableJogControls(MachineOperations& ops);
  void DisableJogControls(MachineOperations& ops);
  void UpdatePosition(MachineOperations& ops);
  bool CheckSafetyLimits() const;
  void OnDoneClicked();
  void OnCancelClicked();
  bool CheckTimeout() const;
  void RenderPositionDisplay();
  void RenderChecklist();
  void RenderJogButtons(MachineOperations& ops);


};

// Global registry for active manual adjustment operations
// This allows the main UI loop to render them
class ManualAdjustmentRegistry {
public:
  static ManualAdjustmentRegistry& GetInstance() {
    static ManualAdjustmentRegistry instance;
    return instance;
  }

  void RegisterOperation(ManualAdjustmentOperation* op) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeOperations.push_back(op);
  }

  void UnregisterOperation(ManualAdjustmentOperation* op) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeOperations.erase(
      std::remove(m_activeOperations.begin(), m_activeOperations.end(), op),
      m_activeOperations.end()
    );
  }

  void RenderAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto* op : m_activeOperations) {
      if (op && op->IsActive()) {
        op->RenderImGui();
      }
    }
  }

private:
  std::vector<ManualAdjustmentOperation*> m_activeOperations;
  std::mutex m_mutex;
};

#endif // MANUAL_ADJUSTMENT_OPERATION_H