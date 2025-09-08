
// Fixed grid_volume_scanner_ui.h
#pragma once
#include "grid_volume_scanner.h"
#include "PIScanMotionAdapter.h"
#include "include/motions/pi_controller_manager.h"
#include "include/data/data_client_manager.h"
#include "imgui.h"
#include "implot/implot.h"
#include "../mainUI/MenuManager_uaa3.h"
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

class GridVolumeScannerUI : public IImguiUI {  // FIX: Added 'public' keyword
public:
  GridVolumeScannerUI();
  ~GridVolumeScannerUI() = default;

  // IImguiUI interface implementation
  void Render() override;
  void Show() override { m_visible = true; }
  void Hide() override { m_visible = false; }
  bool IsVisible() const override { return m_visible; }

  // FIX: GetName() should return const std::string& not std::string&
  const std::string& GetName() const override {
    static std::string name = "Grid Volume Scanner";
    return name;
  }

  void Toggle() override {
    if (IsVisible()) {
      Hide();
    }
    else {
      Show();
    }
  }

  // Set the scanner instance
  void SetScanner(std::shared_ptr<GridVolumeScanner> scanner);

  // Set device and channel names for display
  void SetDeviceInfo(const std::string& deviceName, const std::string& dataChannel);

  // Set managers for reconnection
  void SetManagers(PIControllerManager* piManager, DataClientManager* dataManager) {
    m_piManager = piManager;
    m_dataManager = dataManager;
  }

  void TestZMovement();

private:
  // All your existing private members remain the same
  std::shared_ptr<GridVolumeScanner> m_scanner;
  PIControllerManager* m_piManager = nullptr;
  DataClientManager* m_dataManager = nullptr;
  bool m_visible = false;
  std::string m_deviceName = "Unknown";
  std::string m_dataChannel = "Unknown";
  float m_xStep = 50.0f;
  float m_yStep = 10.0f;
  int m_xPoints = 5;
  int m_yPoints = 5;
  int m_settlingTime = 100;
  int m_zDirection = 1;
  float m_zStepSize = 5.0f;
  int m_zSteps = 10;
  VolumeScanData m_volumeData;
  int m_currentLayer = 0;
  std::vector<std::vector<double>> m_currentLayerData;
  float m_colorScaleMin = 0.0f;
  float m_colorScaleMax = 1.0f;
  bool m_autoScale = true;
  int m_colormapIndex = 0;
  bool m_scanInProgress = false;
  float m_scanProgress = 0.0f;
  int m_currentScanLayer = 0;
  double m_currentZ = 0.0;
  bool m_showResultsDialog = false;
  VolumeGridPoint m_lastPeakPosition;
  double m_lastPeakValue = 0.0;
  double m_lastScanDuration = 0.0;
  std::mutex m_layerDataMutex;
  std::atomic<bool> m_dataUpdated{ false };
  std::atomic<int> m_latestCompletedLayer{ -1 };

  void RenderControls();
  void RenderLayerView();
  void RenderVolumeStats();
  void RenderResultsDialog();
  void CheckAndReconnectHardware();
  void UpdateLayerDisplay(int layer);
  void ResetVolumeData();
  void CalculateZRange(float& zStart, float& zEnd) const;
  void OnLayerCompleted(int layerIndex, const std::vector<std::vector<double>>& layerData, double z);
};