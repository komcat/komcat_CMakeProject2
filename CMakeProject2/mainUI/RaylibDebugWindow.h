#pragma once
#include <memory>
#include <string>
class CameraManager;
class RaylibWindow;
class CameraFeedDisplay;
class Logger;

class RaylibDebugWindow {
public:
  RaylibDebugWindow();
  ~RaylibDebugWindow();

  void SetCameraManager(CameraManager* cameraManager);
  void SetRaylibWindow(RaylibWindow* raylibWindow);
  void SetCameraFeedDisplay(CameraFeedDisplay* cameraFeed);
  void SetLogger(Logger* logger);

  void RenderUI();

private:
  CameraManager* m_cameraManager = nullptr;
  RaylibWindow* m_raylibWindow = nullptr;
  CameraFeedDisplay* m_cameraFeedDisplay = nullptr;
  Logger* m_logger = nullptr;

  std::string m_selectedCameraId;

  void RenderCameraSelection();
  void RenderCameraControls();
  void RenderFeedControls();
  void RenderDebugInfo();
  void RenderQuickActions();
};