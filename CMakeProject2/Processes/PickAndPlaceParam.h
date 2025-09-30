#pragma once
#include "AppSettings.h"
#include "ParameterLoader.h"
#include <string>

// Parameter class for PickAndPlace
class PickAndPlaceParam : public ParameterLoader<PickAndPlaceParam> {
public:
  PickAndPlaceParam(const std::string& deviceName)
    : ParameterLoader("PickAndPlace_" + deviceName) {
    initDefaults();
    loadFromDatabase();
  }

  // Initialize with default values (only creates if they don't exist)
  void initDefaults() override {
    AppSettings& settings = AppSettings::getInstance();
    std::string cat = getCategory();

    if (!settings.exists(cat, "deviceName")) {
      std::cout << "Creating deviceName as STRING" << std::endl;
      settings.createString(cat, "deviceName", "DefaultDevice");
    }

    if (!settings.exists(cat, "speed")) {
      std::cout << "Creating speed as FLOAT" << std::endl;
      settings.createFloat(cat, "speed", 10.0f);
    }

    if (!settings.exists(cat, "nodePick")) {
      std::cout << "Creating nodePick as STRING" << std::endl;
      settings.createString(cat, "nodePick", "PickNode");
    }

    if (!settings.exists(cat, "nodePlace")) {
      std::cout << "Creating nodePlace as STRING" << std::endl;
      settings.createString(cat, "nodePlace", "PlaceNode");
    }

    if (!settings.exists(cat, "enabled")) {
      std::cout << "Creating enabled as BOOL" << std::endl;
      settings.createBool(cat, "enabled", true);
    }
  }

  // Load existing values from database into member variables
  void loadFromDatabase() override {
    // CategoryConfig methods automatically use AppSettings::getInstance()
    m_deviceName = getString("deviceName", "DefaultDevice");
    m_speed = static_cast<double>(getFloat("speed", 10.0f));
    m_nodePick = getString("nodePick", "PickNode");
    m_nodePlace = getString("nodePlace", "PlaceNode");
    m_enabled = getBool("enabled", true);
  }

  // Save current member variables to database
  void saveToDatabase() override {
    // CategoryConfig methods automatically use AppSettings::getInstance()
    setString("deviceName", m_deviceName);
    setFloat("speed", static_cast<float>(m_speed));
    setString("nodePick", m_nodePick);
    setString("nodePlace", m_nodePlace);
    setBool("enabled", m_enabled);
  }

  // Getters - now using cached member variables (const-safe)
  std::string getDeviceName() const {
    return m_deviceName;
  }

  double getSpeed() const {
    return m_speed;
  }

  std::string getNodePick() const {
    return m_nodePick;
  }

  std::string getNodePlace() const {
    return m_nodePlace;
  }

  bool isEnabled() const {
    return m_enabled;
  }

  // Setters - update both member variable and database
  void setDeviceName(const std::string& name) {
    m_deviceName = name;
    setString("deviceName", name);
  }

  void setSpeed(double speed) {
    m_speed = speed;
    setFloat("speed", static_cast<float>(speed));
  }

  void setNodePick(const std::string& node) {
    m_nodePick = node;
    setString("nodePick", node);
  }

  void setNodePlace(const std::string& node) {
    m_nodePlace = node;
    setString("nodePlace", node);
  }

  void setEnabled(bool enabled) {
    m_enabled = enabled;
    setBool("enabled", enabled);
  }

private:
  // Cached values from database
  std::string m_deviceName;
  double m_speed;
  std::string m_nodePick;
  std::string m_nodePlace;
  bool m_enabled;
};