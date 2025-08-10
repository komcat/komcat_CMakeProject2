// ModuleAlignmentUI.h - UI interface for module alignment operations
#pragma once

#include "ModuleAlignment.h"
#include "imgui.h"
#include <memory>
#include <string>
#include <vector>
#include <future>
#include <chrono>

/**
 * @brief UI class for ModuleAlignment operations
 *
 * Provides user interface for:
 * - Running 3-point alignment calibration
 * - Monitoring alignment progress
 * - Saving/loading alignment configurations
 * - Displaying alignment results and coordinate transformations
 * - Testing coordinate conversions
 */
class ModuleAlignmentUI {
public:
  // =============================================================================
  // CONSTRUCTION & LIFECYCLE
  // =============================================================================

  ModuleAlignmentUI();
  ~ModuleAlignmentUI();

  // Disable copy/move
  ModuleAlignmentUI(const ModuleAlignmentUI&) = delete;
  ModuleAlignmentUI& operator=(const ModuleAlignmentUI&) = delete;

  // =============================================================================
  // MAIN UI INTERFACE
  // =============================================================================

  /**
   * @brief Main UI rendering method
   */
  void RenderUI();

  /**
   * @brief Toggle window visibility
   */
  void ToggleWindow() { m_showWindow = !m_showWindow; }

  /**
   * @brief Check if window is visible
   */
  bool IsVisible() const { return m_showWindow; }

  /**
   * @brief Set window visibility
   */
  void SetVisible(bool visible) { m_showWindow = visible; }

  /**
   * @brief Set machine operations reference
   */
  void SetMachineOperations(MachineOperations* machineOps);

  /**
   * @brief Set camera manager reference (optional)
   */
  void SetCameraManager(CameraManager* cameraManager);

private:
  // =============================================================================
  // UI STATE MANAGEMENT
  // =============================================================================

  enum class AlignmentState {
    IDLE,           // Ready to start alignment
    RUNNING,        // Alignment in progress
    COMPLETED,      // Alignment completed successfully
    FAILED          // Alignment failed
  };

  // =============================================================================
  // MEMBER VARIABLES
  // =============================================================================

  // Core system
  std::unique_ptr<ModuleAlignment> m_moduleAlignment;

  // UI state
  bool m_showWindow = true;
  AlignmentState m_currentState = AlignmentState::IDLE;

  // Alignment configuration
  char m_node1Name[64] = "calib_node_1";
  char m_node2Name[64] = "calib_node_2";
  char m_node3Name[64] = "calib_node_3";
  bool m_useRobotZ = false;

  // Progress tracking
  std::future<ModuleAlignment::AlignmentResult> m_alignmentFuture;
  float m_progressValue = 0.0f;
  std::string m_currentStep = "";
  std::string m_statusMessage = "";
  std::chrono::steady_clock::time_point m_alignmentStartTime;

  // Results display
  ModuleAlignment::AlignmentResult m_lastResult;
  bool m_hasResult = false;

  // Saved alignments management
  std::vector<std::string> m_savedAlignments;
  int m_selectedAlignment = -1;
  char m_saveAlignmentName[64] = "alignment_v1";
  bool m_showSaveDialog = false;
  bool m_showLoadDialog = false;
  bool m_showDeleteDialog = false;

  // Coordinate transformation testing
  bool m_showTransformTest = false;
  float m_testMachinePos[3] = { 100.0f, 50.0f, 25.0f };
  float m_testAlignmentPos[3] = { 0.0f, 0.0f, 0.0f };

  // Display options
  bool m_showAdvancedSettings = false;
  bool m_showResultDetails = false;
  // UI Tab Control
  int m_activeTab = 0;                   // 0=Config, 1=Results, 2=Saved, 3=Transform, 4=Advanced
  bool m_autoSwitchToResults = true;     // Automatically switch to Results tab when alignment completes

  // =============================================================================
  // PRIVATE METHODS
  // =============================================================================

  // UI Rendering sections
  void RenderAlignmentConfiguration();
  void RenderProgressSection();
  void RenderResultsSection();
  void RenderSavedAlignmentsSection();
  void RenderTransformationTestSection();
  void RenderAdvancedSettings();

  // Dialog rendering
  void RenderSaveDialog();
  void RenderLoadDialog();
  void RenderDeleteConfirmDialog();

  // Utility methods
  void StartAlignment();
  void UpdateProgress();
  void RefreshSavedAlignments();
  void ResetAlignment();

  // Status helpers
  bool IsAlignmentRunning() const;
  std::string FormatPosition(const PositionStruct& pos) const;
  std::string FormatVector(const ModuleAlignment::Vector3D& vec) const;
  ImVec4 GetStateColor(AlignmentState state) const;
  const char* GetStateText(AlignmentState state) const;


  // Helper function for angle calculation
  double CalculateAngleBetweenVectors(const ModuleAlignment::Vector3D& v1,
    const ModuleAlignment::Vector3D& v2) const;
};