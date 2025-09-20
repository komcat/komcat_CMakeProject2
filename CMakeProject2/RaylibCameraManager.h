#pragma once

#include <memory>
#include <string>
#include "AppContext.h"
#include "CameraFeedDisplay.h"
#include "raylibclass.h"
#include "RaylibDebugWindow.h"
#include "MenuManager_uaa3.h"
#include "include/logger.h"
#include "include/data/global_data_store.h"

/**
 * @brief Manages Raylib 3D window and camera feed integration using AppContext
 *
 * This class encapsulates all the raylib and camera feed setup logic that was
 * previously scattered throughout the main application. It uses AppContext for
 * service discovery and provides a clean interface for raylib operations.
 */
class RaylibCameraManager {
public:
  struct InitializationOptions {
    bool enableRaylib3D = true;
    bool autoConnectCamera = true;
    bool enableDebugWindow = true;
    int cameraConnectionTimeout = 500; // milliseconds
  };

  /**
   * @brief Constructor taking AppContext reference
   */
  explicit RaylibCameraManager(AppContext& context, Logger* logger = nullptr);

  /**
   * @brief Destructor - handles cleanup
   */
  ~RaylibCameraManager();

  /**
   * @brief Initialize the raylib system with camera integration
   * @param options Configuration options for initialization
   * @return true if initialization succeeded, false otherwise
   */
  bool Initialize(const InitializationOptions& options = {});

  /**
   * @brief Shutdown the raylib system cleanly
   */
  void Shutdown();

  /**
   * @brief Update raylib window with current machine data (call in main loop)
   * @param data Current machine data to display
   */
  void UpdateMachineData(const MachineData& data);

  /**
   * @brief Render debug UI if enabled (call in main loop)
   * @param menuManager Menu manager for visibility control
   */
  void RenderDebugUI(MenuManagerUaa3* menuManager);

  /**
   * @brief Check if raylib window should close
   * @return true if window should close
   */
  bool ShouldClose() const;

  /**
   * @brief Check if raylib is running
   * @return true if raylib window is active
   */
  bool IsRunning() const;

  /**
   * @brief Get the camera feed display (for external use)
   * @return Pointer to camera feed display, nullptr if not initialized
   */
  CameraFeedDisplay* GetCameraFeedDisplay() const { return m_raylibCameraFeed.get(); }

  /**
   * @brief Get the raylib window (for external use)
   * @return Pointer to raylib window, nullptr if not initialized
   */
  RaylibWindow* GetRaylibWindow() const { return m_raylibWindow.get(); }

  /**
   * @brief Get the debug window (for external use)
   * @return Pointer to debug window, nullptr if not initialized
   */
  RaylibDebugWindow* GetDebugWindow() const { return m_raylibDebugWindow.get(); }

  /**
   * @brief Get initialization status
   * @return true if successfully initialized
   */
  bool IsInitialized() const { return m_initialized; }

  /**
   * @brief Get last error message
   * @return Error message string
   */
  const std::string& GetLastError() const { return m_lastError; }

  /**
   * @brief Set global data store for raylib integration
   * @param dataStore Pointer to global data store
   */
  void SetDataStore(GlobalDataStore* dataStore) { m_dataStore = dataStore; }

private:
  // Core references
  AppContext& m_context;
  Logger* m_logger;
  GlobalDataStore* m_dataStore = nullptr;

  // Raylib components
  std::unique_ptr<CameraFeedDisplay> m_raylibCameraFeed;
  std::unique_ptr<RaylibWindow> m_raylibWindow;
  std::unique_ptr<RaylibDebugWindow> m_raylibDebugWindow;

  // State tracking
  bool m_initialized = false;
  std::string m_lastError;
  std::string m_connectedCameraId;

  // Helper methods
  bool SetupCameraFeed();
  bool SetupRaylibWindow();
  bool SetupDebugWindow();
  bool AutoConnectCamera();
  void LogError(const std::string& message);
  void LogInfo(const std::string& message);

  // Enhanced debug function
  void DebugCameraFeedSetup();
};