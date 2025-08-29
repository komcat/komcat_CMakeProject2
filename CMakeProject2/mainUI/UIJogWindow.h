// UIJogWindow.h - Updated with position subscription support
#pragma once

#include "include/motions/global_jog_panel.h"
#include "include/motions/IPositionSubscriber.h"
#include <memory>
#include <mutex>
#include <map>
#include <vector>

// Forward declarations
class MotionConfigManager;
class PIControllerManager;
class ACSControllerManager;

class UIJogWindow : public IPositionSubscriber {
public:
  // Constructor with managers (for real operation)
  UIJogWindow(MotionConfigManager& configManager,
    PIControllerManager& piControllerManager,
    ACSControllerManager& acsControllerManager);

  // Constructor without managers (for testing/mock mode)
  UIJogWindow(MotionConfigManager& configManager);

  ~UIJogWindow();

  // Disable copy/move
  UIJogWindow(const UIJogWindow&) = delete;
  UIJogWindow& operator=(const UIJogWindow&) = delete;
  UIJogWindow(UIJogWindow&&) = delete;
  UIJogWindow& operator=(UIJogWindow&&) = delete;

  // IPositionSubscriber implementation
  void OnPositionsUpdate(const std::string& deviceName,
    const std::map<std::string, double>& positions) override;
  void OnMotionStatusChange(const std::string& deviceName,
    const std::string& axis, bool isMoving) override;

  // Window control
  void ToggleWindow();
  bool IsVisible() const;
  void SetVisible(bool visible);

  // Render - forwards to GlobalJogPanel but sets predetermined position
  void RenderUI();

  // Key input forwarding  
  void ProcessKeyInput(int keyCode, bool keyDown);

  // Set predetermined position
  void SetPredeterminedPosition();

  // Methods to set controller managers (called from MainUIManager)
  void SetPIControllerManager(PIControllerManager* piManager);
  void SetACSControllerManager(ACSControllerManager* acsManager);

  // Get cached positions for a device
  std::map<std::string, double> GetCachedPositions(const std::string& deviceName) const;

private:
  // References to managers
  MotionConfigManager& m_configManager;
  PIControllerManager* m_piControllerManager = nullptr;
  ACSControllerManager* m_acsControllerManager = nullptr;

  // GlobalJogPanel instance (created when controllers are available)
  std::unique_ptr<GlobalJogPanel> m_globalJogPanel;

  // Window management
  bool m_firstRender = true;

  // Mock functionality for testing without controllers
  bool m_mockWindowVisible = false;
  void RenderMockJogWindow();

  // Helper to create GlobalJogPanel when managers are available
  void CreateGlobalJogPanel();

  // Position caching for real-time updates
  mutable std::mutex m_positionMutex;
  std::map<std::string, std::map<std::string, double>> m_cachedPositions;
  std::map<std::string, std::map<std::string, bool>> m_cachedMotionStatus;

  // Track subscriptions
  std::vector<std::string> m_subscribedPIDevices;
  std::vector<std::string> m_subscribedACSDevices;

  // Helper methods for subscription management
  void SubscribeToPIControllers();
  void UnsubscribeFromPIControllers();
  void SubscribeToACSControllers();
  void UnsubscribeFromACSControllers();
};