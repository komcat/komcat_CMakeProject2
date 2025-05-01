// Adapters for controller managers

#include "include/ui/ToolbarMenu.h"
#include "include/motions/acs_controller_manager.h"
#include "include/motions/pi_controller_manager.h"

// Adapter for ACSControllerManager
// This class manages visibility of individual ACS controllers
class ACSControllerManagerAdapter : public ITogglableUI {
public:
  ACSControllerManagerAdapter(ACSControllerManager& manager, const std::string& name)
    : m_manager(manager), m_name(name), m_isVisible(false) {
  }

  bool IsVisible() const override {
    return m_isVisible;
  }

  void ToggleWindow() override {
    m_isVisible = !m_isVisible;

    // Toggle visibility for the manager's controllers directly
    // Since we don't have a direct way to get all controllers or their visibility,
    // we'll implement a simplified version

    // Create a list of known ACS controller device names (could be stored in a config)
    static const std::vector<std::string> acsDeviceNames = { "gantry-main" };

    // Toggle visibility for each controller
    for (const auto& deviceName : acsDeviceNames) {
      ACSController* controller = m_manager.GetController(deviceName);
      if (controller) {
        controller->SetWindowVisible(m_isVisible);
      }
    }
  }

  const std::string& GetName() const override {
    return m_name;
  }

private:
  ACSControllerManager& m_manager;
  std::string m_name;
  bool m_isVisible;
};

// Adapter for PIControllerManager
// This class manages visibility of individual PI controllers
class PIControllerManagerAdapter : public ITogglableUI {
public:
  PIControllerManagerAdapter(PIControllerManager& manager, const std::string& name)
    : m_manager(manager), m_name(name), m_isVisible(false) {
  }

  bool IsVisible() const override {
    return m_isVisible;
  }

  void ToggleWindow() override {
    m_isVisible = !m_isVisible;

    // Toggle visibility for the manager's controllers directly
    // Since we don't have a direct way to get all controllers or their visibility,
    // we'll implement a simplified version

    // Create a list of known PI controller device names (could be stored in a config)
    static const std::vector<std::string> piDeviceNames = { "hex-left", "hex-right", "hex-bottom" };

    // Toggle visibility for each controller
    for (const auto& deviceName : piDeviceNames) {
      PIController* controller = m_manager.GetController(deviceName);
      if (controller) {
        controller->SetWindowVisible(m_isVisible);
      }
    }
  }

  const std::string& GetName() const override {
    return m_name;
  }

private:
  PIControllerManager& m_manager;
  std::string m_name;
  bool m_isVisible;
};

// Helper functions to create the adapters
std::shared_ptr<ITogglableUI> CreateACSControllerAdapter(ACSControllerManager& manager, const std::string& name) {
  return std::make_shared<ACSControllerManagerAdapter>(manager, name);
}

std::shared_ptr<ITogglableUI> CreatePIControllerAdapter(PIControllerManager& manager, const std::string& name) {
  return std::make_shared<PIControllerManagerAdapter>(manager, name);
}