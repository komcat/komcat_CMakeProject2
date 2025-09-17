#pragma once
#include "GlobalMotionController.h"
#include "imgui.h"
#include <string>
#include <functional>

class EmbeddedJogControl {
public:
  explicit EmbeddedJogControl(GlobalMotionController& motionController);
  ~EmbeddedJogControl() = default;

  // Main render function
  void Render();
  void Render(const std::string& title);

  // Configuration
  void SetCompactMode(bool compact) { m_compactMode = compact; }
  void SetShowTransformMatrix(bool show) { m_showTransformMatrix = show; }
  void SetShowPositionDisplay(bool show) { m_showPositionDisplay = show; }
  void SetDefaultStepSize(float size) { m_jogState.stepSize = size; }
  void SetDefaultVelocity(float vel) { m_jogState.velocity = vel; }

  // Callbacks
  using StatusCallback = std::function<void(const std::string&)>;
  void SetStatusCallback(StatusCallback callback) { m_statusCallback = callback; }

  // State queries
  bool IsMoving() const { return m_isMoving; }
  std::string GetActiveDevice() const { return m_jogState.activeDeviceId; }

private:
  GlobalMotionController& m_motionController;
  bool m_keyboardEnabled = false;

  // Add new method:
  void ProcessKeyboardInput();


  struct JogState {
    int selectedDevice = 0;
    float stepSize = 0.01f;
    float velocity = 5.0f; //5mm/s
    std::string activeDeviceId = "hex-left";
  } m_jogState;

  bool m_compactMode = false;
  bool m_showTransformMatrix = false;
  bool m_showPositionDisplay = true;
  bool m_isMoving = false;

  StatusCallback m_statusCallback;

  // Render methods
  void RenderDeviceSelector();
  void RenderStepControls();
  void RenderJogButtons();
  void RenderCompactJogButtons();
  void RenderPositionDisplay();
  void RenderTransformMatrixInfo();
  void RenderStopButton();


  // Helper methods
  void HandleJogMovement(int axis, float distance);
  std::string GetDeviceIdFromSelection(int selection);
  void UpdateStatus(const std::string& message);
};