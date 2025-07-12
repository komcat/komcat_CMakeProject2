#pragma once

#include <memory>

// Forward declarations
class MotionConfigManager;
class UIConfigEditor;
class UIConfigVisualizer;  // Add this forward declaration

class MainUIManager {
public:
  enum class MainPage {
    MAIN,
    MANUAL,
    DATA_INSTRUMENT,
    RUN_PROGRAM,
    CONFIG,
    VISION
  };

  enum class ManualSubPage {
    NONE,
    PI,
    GANTRY,
    IO,
    CAMERA
  };

  enum class ConfigSubPage {
    NONE,
    CONFIG_EDITOR,
    NODE_VISUALIZER
  };

private:
  MainPage currentMainPage = MainPage::MAIN;
  ManualSubPage currentManualSubPage = ManualSubPage::NONE;
  ConfigSubPage currentConfigSubPage = ConfigSubPage::NONE;

  // Reference to the config manager (owned by main)
  MotionConfigManager& motionConfigManager;

  // UI components we own
  std::unique_ptr<UIConfigEditor> uiConfigEditor;
  std::unique_ptr<UIConfigVisualizer> uiConfigVisualizer;  // Add this

public:
  // Constructor takes MotionConfigManager reference
  MainUIManager(MotionConfigManager& configManager);
  ~MainUIManager();

  void RenderUI();

private:
  void RenderTopMenuBar();
  void RenderDateTime();
  void RenderBreadcrumbs();
  void RenderMainContent();

  // Main pages
  void RenderMainPage();
  void RenderManualPage();
  void RenderDataInstrumentPage();
  void RenderRunProgramPage();
  void RenderConfigPage();
  void RenderVisionPage();

  // Manual sub-pages
  void RenderManualSubPage();
  void RenderPIPage();
  void RenderGantryPage();
  void RenderIOPage();
  void RenderCameraPage();

  // Config sub-pages
  void RenderConfigSubPage();
  void RenderConfigEditorPage();
  void RenderNodeVisualizerPage();
};