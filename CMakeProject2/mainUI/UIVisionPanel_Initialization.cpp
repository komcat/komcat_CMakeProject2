// UIVisionPanel_Initialization.cpp - System initialization methods
#include "UIVisionPanel.h"
#include "include/halcon/VisionCircleDetection.h"
#include <iostream>

void UIVisionPanel::InitializeCircleDetection() {
  m_circleDetector = std::make_unique<VisionCircleDetection>();

  if (!m_circleDetector->LoadParameters(m_parameterFilePath)) {
    std::cout << "[UIVisionPanel] Creating default parameter file" << std::endl;
    if (VisionCircleDetection::CreateDefaultParameterFile(m_parameterFilePath)) {
      m_circleDetector->LoadParameters(m_parameterFilePath);
    }
  }

  std::cout << "[UIVisionPanel] Circle detection initialized successfully" << std::endl;
}

void UIVisionPanel::InitializePresetManager() {
  try {
    m_presetManager = std::make_unique<VisionPresetManager>();
    if (m_presetManager->Initialize()) {
      RefreshPresetList();
      std::cout << "[UIVisionPanel] Preset manager initialized with "
        << m_availablePresets.size() << " presets" << std::endl;

      // Optionally load the first default preset
      if (!m_availablePresets.empty()) {
        for (const auto& preset : m_availablePresets) {
          if (preset.isDefault && preset.name == "Medium Circle") {
            LoadPreset(preset.id);
            m_selectedPresetId = preset.id;
            std::cout << "[UIVisionPanel] Auto-loaded default preset: " << preset.name << std::endl;
            break;
          }
        }
      }
    }
    else {
      std::cerr << "[UIVisionPanel] Failed to initialize preset manager: "
        << m_presetManager->GetLastError() << std::endl;
    }
  }
  catch (const std::exception& e) {
    std::cerr << "[UIVisionPanel] Exception initializing preset manager: " << e.what() << std::endl;
  }
}