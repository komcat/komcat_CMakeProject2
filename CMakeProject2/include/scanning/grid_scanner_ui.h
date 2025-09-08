// Fixed grid_scanner_ui.h
#pragma once
#include "include/scanning/PIScanMotionAdapter.h"
#include "include/motions/pi_controller_manager.h"
#include "include/scanning/grid_scanner.h"
#include "../mainUI/MenuManager_uaa3.h"
#include "imgui.h"
#include "implot/implot.h"
#include <memory>
#include <vector>
#include <string>

class GridScannerUI : public IImguiUI {  // FIX: Added 'public' keyword
public:
  GridScannerUI();
  ~GridScannerUI() = default;

  // IImguiUI interface implementation
  void Render() override;
  void Show() override { m_visible = true; }
  void Hide() override { m_visible = false; }
  bool IsVisible() const override { return m_visible; }

  // FIX: GetName() should return const std::string& not std::string&
  const std::string& GetName() const override {
    static std::string name = "Grid Scanner";
    return name;
  }

  void Toggle() override {
    if (IsVisible()) {
      Hide();
    }
    else {
      Show();
      CheckAndReconnectHardware();
    }
  }

  // Set the scanner instance
  void SetScanner(std::shared_ptr<GridScanner> scanner);

  // Set device and channel names for display
  void SetDeviceInfo(const std::string& deviceName, const std::string& dataChannel);

  // Set managers for reconnection
  void SetManagers(PIControllerManager* piManager, DataClientManager* dataManager) {
    m_piManager = piManager;
    m_dataManager = dataManager;
  }

private:
  std::shared_ptr<GridScanner> m_scanner;
  bool m_visible = false;
  std::string m_deviceName;
  std::string m_dataChannel;
  std::vector<std::vector<double>> m_heatmapData;
  GridPoint m_currentPosition;
  double m_currentValue;
  bool m_dataUpdated;
  float m_xStep;
  float m_yStep;
  int m_xPoints;
  int m_yPoints;
  int m_settlingTime;
  float m_colorScaleMin;
  float m_colorScaleMax;
  bool m_autoScale;
  int m_colormapIndex;
  bool m_scanInProgress;
  float m_scanProgress;
  PIControllerManager* m_piManager = nullptr;
  DataClientManager* m_dataManager = nullptr;
  bool m_showResultsDialog = false;
  double m_lastPeakValue = 0.0;
  GridPoint m_lastPeakPosition;

  void UpdateHeatmap(const GridPoint& point, double value);
  void RenderControls();
  void RenderHeatmap();
  void RenderStatus();
  void ResetHeatmapData();
  void CheckAndReconnectHardware();
  void RenderResultsDialog();
};