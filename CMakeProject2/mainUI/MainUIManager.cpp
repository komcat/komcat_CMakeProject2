#include "MainUIManager.h"
#include "UIConfigEditor.h"
#include "UIConfigVisualizer.h"
#include "UIJogWindow.h"
#include "include/motions/MotionConfigManager.h"
#include "include/motions/pi_controller_manager.h"
#include "include/motions/acs_controller_manager.h"
// Add this include at the top:
#include "PIPanelUI.h"
#include "imgui.h"
#include <iostream>
#include <string>
#include <ctime>

MainUIManager::MainUIManager(MotionConfigManager& configMgr)
  : motionConfigManager(configMgr),
  m_piControllerManager(nullptr),
  m_acsControllerManager(nullptr) {

  // Create UIConfigEditor with the config manager reference
  uiConfigEditor = std::make_unique<UIConfigEditor>(motionConfigManager);

  // Create UIConfigVisualizer with the config manager reference
  uiConfigVisualizer = std::make_unique<UIConfigVisualizer>(motionConfigManager);

  // Create UIJogWindow using the single-parameter constructor for mock mode
  m_uiJogWindow = std::make_unique<UIJogWindow>(motionConfigManager);
}

MainUIManager::~MainUIManager() = default;

void MainUIManager::SetMotionManagers(PIControllerManager* piManager, ACSControllerManager* acsManager) {
  m_piControllerManager = piManager;
  m_acsControllerManager = acsManager;

  // Create the PI Panel UI when PI manager is available
  if (m_piControllerManager) {
    m_piPanelUI = std::make_unique<PIPanelUI>(*m_piControllerManager);
  }

  // Now create the UIJogWindow
  if (m_piControllerManager && m_acsControllerManager) {
    m_uiJogWindow = std::make_unique<UIJogWindow>(
      motionConfigManager, *m_piControllerManager, *m_acsControllerManager);
  }



}

void MainUIManager::RenderUI() {
  // Main window that fills the entire screen
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::Begin("MainApplication", nullptr, window_flags);

  // Only show top menu bar when on main page
  if (currentMainPage == MainPage::MAIN) {
    RenderTopMenuBar();
  }
  else {
    // Show back button in top-left when not on main page
    RenderBackButton();
  }

  RenderDateTime();
  RenderBreadcrumbs();
  ImGui::Separator();
  RenderMainContent();

  ImGui::End();


  // Always render jog window (it handles its own visibility internally)
  RenderGlobalJogWindow();
}

void MainUIManager::RenderBackButton() {
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15, 8));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

  if (ImGui::Button("<< BACK")) {
    if (currentManualSubPage != ManualSubPage::NONE) {
      // Go back to Manual main page
      currentManualSubPage = ManualSubPage::NONE;
    }
    else if (currentConfigSubPage != ConfigSubPage::NONE) {
      // Go back to Config main page
      currentConfigSubPage = ConfigSubPage::NONE;
    }
    else {
      // Go back to main page
      currentMainPage = MainPage::MAIN;
    }
  }

  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar();
}


void MainUIManager::RenderTopMenuBar() {
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 10));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.3f, 0.8f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.4f, 0.9f, 1.0f));

  if (ImGui::Button("Manual", ImVec2(120, 40))) {
    currentMainPage = MainPage::MANUAL;
    currentManualSubPage = ManualSubPage::NONE;
    currentConfigSubPage = ConfigSubPage::NONE;
  }

  ImGui::SameLine();
  if (ImGui::Button("Data & Instrument", ImVec2(150, 40))) {
    currentMainPage = MainPage::DATA_INSTRUMENT;
    currentManualSubPage = ManualSubPage::NONE;
    currentConfigSubPage = ConfigSubPage::NONE;
  }

  ImGui::SameLine();
  if (ImGui::Button("Run Program", ImVec2(120, 40))) {
    currentMainPage = MainPage::RUN_PROGRAM;
    currentManualSubPage = ManualSubPage::NONE;
    currentConfigSubPage = ConfigSubPage::NONE;
  }

  ImGui::SameLine();
  if (ImGui::Button("Config", ImVec2(120, 40))) {
    currentMainPage = MainPage::CONFIG;
    currentManualSubPage = ManualSubPage::NONE;
    currentConfigSubPage = ConfigSubPage::NONE;
  }

  ImGui::SameLine();
  if (ImGui::Button("Vision", ImVec2(120, 40))) {
    currentMainPage = MainPage::VISION;
    currentManualSubPage = ManualSubPage::NONE;
    currentConfigSubPage = ConfigSubPage::NONE;
  }

  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar();
}

void MainUIManager::RenderDateTime() {
  // Get date/time strings
  time_t rawtime;
  struct tm timeinfo;
  char timeBuffer[80];
  char dateBuffer[80];

  time(&rawtime);
#ifdef _WIN32
  localtime_s(&timeinfo, &rawtime);
#else
  timeinfo = *localtime(&rawtime);
#endif

  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
  strftime(dateBuffer, sizeof(dateBuffer), "%d %b %Y", &timeinfo);

  std::string datetime = std::string(dateBuffer) + "\n" + std::string(timeBuffer);

  // Calculate positions
  ImVec2 textSize = ImGui::CalcTextSize(datetime.c_str());
  float buttonWidth = 50.0f;
  float buttonHeight = 30.0f;
  float spacing = 10.0f;

  // Position for JOG button (left of date/time)
  ImGui::SameLine(ImGui::GetWindowWidth() - textSize.x - buttonWidth - spacing - 20);
  ImGui::SetCursorPosY(10);

  // Check if jog window is available
  bool jogAvailable = (m_uiJogWindow != nullptr);



  // Button styling - gray out if not available
  if (jogAvailable && m_showGlobalJogWindow) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f)); // Active green
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
  }
  else if (jogAvailable) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // Inactive gray
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f)); // Disabled dark gray
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
  }

  if (ImGui::Button("JOG", ImVec2(buttonWidth, buttonHeight))) {
    if (m_uiJogWindow) {
      m_uiJogWindow->ToggleWindow();
    }
  }

  ImGui::PopStyleColor(2);

  // Update tooltip based on availability
  if (ImGui::IsItemHovered()) {
    if (jogAvailable) {
      ImGui::SetTooltip("Global Jog Control");
    }
    else {
      ImGui::SetTooltip("Global Jog Control\n(Motion controllers not available)");
    }
  }

  // Date/time display (existing position)
  ImGui::SameLine(ImGui::GetWindowWidth() - textSize.x - 20);
  ImGui::SetCursorPosY(10);
  ImGui::Text("%s", datetime.c_str());
}

void MainUIManager::RenderBreadcrumbs() {
  ImGui::SetCursorPosY(60);

  std::string breadcrumb = "Main Page";

  if (currentMainPage != MainPage::MAIN) {
    breadcrumb += " > ";
    switch (currentMainPage) {
    case MainPage::MANUAL: breadcrumb += "Manual"; break;
    case MainPage::DATA_INSTRUMENT: breadcrumb += "Data & Instrument"; break;
    case MainPage::RUN_PROGRAM: breadcrumb += "Run Program"; break;
    case MainPage::CONFIG: breadcrumb += "Config"; break;
    case MainPage::VISION: breadcrumb += "Vision"; break;
    default: break;
    }

    if (currentManualSubPage != ManualSubPage::NONE) {
      breadcrumb += " > ";
      switch (currentManualSubPage) {
      case ManualSubPage::PI: breadcrumb += "PI"; break;
      case ManualSubPage::GANTRY: breadcrumb += "Gantry"; break;
      case ManualSubPage::IO: breadcrumb += "IO"; break;
      case ManualSubPage::CAMERA: breadcrumb += "Camera"; break;
      default: break;
      }
    }

    // Add config sub-pages
    if (currentConfigSubPage != ConfigSubPage::NONE) {
      breadcrumb += " > ";
      switch (currentConfigSubPage) {
      case ConfigSubPage::CONFIG_EDITOR: breadcrumb += "Config Editor"; break;
      case ConfigSubPage::NODE_VISUALIZER: breadcrumb += "Node Visualizer"; break;
      default: break;
      }
    }
  }

  // Display breadcrumb text
  ImGui::Text("%s", breadcrumb.c_str());

  //// Show back button BELOW the breadcrumb if not on main page
  //if (currentMainPage != MainPage::MAIN) {
  //  if (ImGui::Button("<< BACK")) {
  //    if (currentManualSubPage != ManualSubPage::NONE) {
  //      // Go back to Manual main page
  //      currentManualSubPage = ManualSubPage::NONE;
  //    }
  //    else if (currentConfigSubPage != ConfigSubPage::NONE) {
  //      // Go back to Config main page
  //      currentConfigSubPage = ConfigSubPage::NONE;
  //    }
  //    else {
  //      // Go back to main page
  //      currentMainPage = MainPage::MAIN;
  //    }
  //  }
  //}
}

void MainUIManager::RenderMainContent() {
  ImGui::SetCursorPosY(100);

  if (currentMainPage == MainPage::MAIN) {
    RenderMainPage();
  }
  else if (currentMainPage == MainPage::MANUAL) {
    if (currentManualSubPage == ManualSubPage::NONE) {
      RenderManualPage();
    }
    else {
      RenderManualSubPage();
    }
  }
  else if (currentMainPage == MainPage::DATA_INSTRUMENT) {
    RenderDataInstrumentPage();
  }
  else if (currentMainPage == MainPage::RUN_PROGRAM) {
    RenderRunProgramPage();
  }
  else if (currentMainPage == MainPage::CONFIG) {
    if (currentConfigSubPage == ConfigSubPage::NONE) {
      RenderConfigPage();
    }
    else {
      RenderConfigSubPage();
    }
  }
  else if (currentMainPage == MainPage::VISION) {
    RenderVisionPage();
  }
}

void MainUIManager::RenderMainPage() {
  ImGui::SetWindowFontScale(2.0f);
  ImGui::Text("Welcome to uaa3App");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Select a category from the top menu to begin:");
  ImGui::BulletText("Manual - Direct control of hardware components");
  ImGui::BulletText("Data & Instrument - Data monitoring and instruments");
  ImGui::BulletText("Run Program - Execute automated sequences");
  ImGui::BulletText("Config - System configuration and settings");
  ImGui::BulletText("Vision - Image processing and computer vision");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Simple status display
  ImGui::SetWindowFontScale(1.3f);
  ImGui::Text("System Status");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Motion Config Manager: Ready");
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ UI Config Editor: Ready");
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ UI Config Visualizer: Ready");

  // Show jog status
  if (m_uiJogWindow) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Global Jog Panel: Ready");
  }
  else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "○ Global Jog Panel: Waiting for motion controllers");
  }

  ImGui::Spacing();
  ImGui::Text("All systems operational. You can use the Config Editor to manage motion settings");
  ImGui::Text("and the Node Visualizer to view and edit motion graphs interactively.");
}

void MainUIManager::RenderManualPage() {
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("Manual Control");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Select a hardware component to control:");
  ImGui::Spacing();

  // Create buttons for manual sub-categories
  if (ImGui::Button("1. PI", ImVec2(200, 50))) {
    currentManualSubPage = ManualSubPage::PI;
  }

  if (ImGui::Button("2. Gantry", ImVec2(200, 50))) {
    currentManualSubPage = ManualSubPage::GANTRY;
  }

  if (ImGui::Button("3. IO", ImVec2(200, 50))) {
    currentManualSubPage = ManualSubPage::IO;
  }

  if (ImGui::Button("4. Camera", ImVec2(200, 50))) {
    currentManualSubPage = ManualSubPage::CAMERA;
  }
}

void MainUIManager::RenderManualSubPage() {
  switch (currentManualSubPage) {
  case ManualSubPage::PI:
    RenderPIPage();
    break;
  case ManualSubPage::GANTRY:
    RenderGantryPage();
    break;
  case ManualSubPage::IO:
    RenderIOPage();
    break;
  case ManualSubPage::CAMERA:
    RenderCameraPage();
    break;
  default:
    break;
  }
}

// Replace RenderPIPage() method with:
void MainUIManager::RenderPIPage() {
  if (m_piPanelUI) {
    m_piPanelUI->RenderUI();
  }
  else {
    // Fallback message
    ImGui::Text("PI Controllers not available");
    ImGui::Text("Motion controller managers have not been initialized.");
  }
}

void MainUIManager::RenderGantryPage() {
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("Gantry Controller");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Gantry Controller UI will be implemented here");

  // Placeholder Gantry controls
  ImGui::Separator();
  static bool homed = false;
  if (ImGui::Button("Home Gantry")) {
    homed = true;
    std::cout << "Homing gantry..." << std::endl;
  }

  if (homed) {
    ImGui::Text("Status: Homed");
    static float gantryPos = 0.0f;
    ImGui::SliderFloat("Gantry Position", &gantryPos, 0.0f, 1000.0f);
  }
}

void MainUIManager::RenderIOPage() {
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("IO Control");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("IO Control UI will be implemented here");

  // Placeholder IO controls
  ImGui::Separator();
  static bool outputs[8] = { false };

  for (int i = 0; i < 8; i++) {
    char label[32];
    sprintf(label, "Output %d", i + 1);
    if (ImGui::Checkbox(label, &outputs[i])) {
      std::cout << "Output " << i + 1 << " set to " << (outputs[i] ? "ON" : "OFF") << std::endl;
    }
    if (i % 2 == 1) ImGui::SameLine();
  }
}

void MainUIManager::RenderCameraPage() {
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("Camera Control");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Camera Control UI will be implemented here");

  // Placeholder Camera controls
  ImGui::Separator();
  static float exposure = 1000.0f;
  static float gain = 1.0f;

  ImGui::SliderFloat("Exposure (us)", &exposure, 100.0f, 10000.0f);
  ImGui::SliderFloat("Gain", &gain, 1.0f, 20.0f);

  if (ImGui::Button("Capture Image")) {
    std::cout << "Capturing image with exposure: " << exposure << "us, gain: " << gain << std::endl;
  }
}

void MainUIManager::RenderDataInstrumentPage() {
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("Data & Instrument");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Data monitoring and instrument control will be implemented here");
}

void MainUIManager::RenderRunProgramPage() {
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("Run Program");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Program execution interface will be implemented here");
}

void MainUIManager::RenderConfigPage() {
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("Configuration");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Select a configuration tool:");
  ImGui::Spacing();

  if (ImGui::Button("1. Config Editor", ImVec2(200, 50))) {
    currentConfigSubPage = ConfigSubPage::CONFIG_EDITOR;
  }

  if (ImGui::Button("2. Node Visualizer", ImVec2(200, 50))) {
    currentConfigSubPage = ConfigSubPage::NODE_VISUALIZER;
  }
}

void MainUIManager::RenderVisionPage() {
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("Vision & Image Processing");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Text("Computer vision and image processing tools will be implemented here");

  // Placeholder Vision controls
  ImGui::Separator();
  ImGui::Text("Image Processing Controls:");

  static float threshold = 128.0f;
  static float blur = 1.0f;
  static bool enableEdgeDetection = false;

  ImGui::SliderFloat("Threshold", &threshold, 0.0f, 255.0f);
  ImGui::SliderFloat("Blur Radius", &blur, 0.0f, 10.0f);
  ImGui::Checkbox("Enable Edge Detection", &enableEdgeDetection);

  ImGui::Spacing();
  if (ImGui::Button("Process Image")) {
    std::cout << "Processing image with threshold: " << threshold
      << ", blur: " << blur
      << ", edge detection: " << (enableEdgeDetection ? "ON" : "OFF") << std::endl;
  }

  ImGui::SameLine();
  if (ImGui::Button("Capture & Analyze")) {
    std::cout << "Capturing and analyzing image..." << std::endl;
  }
}

void MainUIManager::RenderConfigSubPage() {
  switch (currentConfigSubPage) {
  case ConfigSubPage::CONFIG_EDITOR:
    RenderConfigEditorPage();
    break;
  case ConfigSubPage::NODE_VISUALIZER:
    RenderNodeVisualizerPage();
    break;
  default:
    break;
  }
}

void MainUIManager::RenderConfigEditorPage() {
  // Super simple - just render the UIConfigEditor!
  if (uiConfigEditor) {
    uiConfigEditor->RenderUI();
  }
  else {
    ImGui::Text("Config Editor not available");
  }
}

void MainUIManager::RenderNodeVisualizerPage() {
  // Replace the placeholder with actual UIConfigVisualizer!
  if (uiConfigVisualizer) {
    uiConfigVisualizer->RenderUI();
  }
  else {
    ImGui::Text("Node Visualizer not available");
  }
}

void MainUIManager::RenderGlobalJogWindow() {
  // Let UIJogWindow handle its own rendering
  if (m_uiJogWindow) {
    m_uiJogWindow->RenderUI();
  }
}