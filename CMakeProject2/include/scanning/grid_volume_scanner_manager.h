// include/scanning/grid_volume_scanner_manager.h
#pragma once

#include "grid_volume_scanner.h"
#include "grid_volume_scanner_ui.h"
#include "PIScanMotionAdapter.h"
#include "include/motions/pi_controller_manager.h"
#include "include/data/data_client_manager.h"

class GridVolumeScannerManager {
public:
  GridVolumeScannerManager()
    : m_piManager(nullptr)
    , m_dataManager(nullptr)
    , m_deviceName("hex-left")
    , m_dataChannel("GPIB-Current") {

    // Create UI immediately
    m_ui = std::make_unique<GridVolumeScannerUI>();

    // Set up device change callback
    m_ui->SetDeviceChangeCallback(
      [this](const std::string& deviceName) {
      this->SetSelectedDevice(deviceName);
    });
  }

  void SetPIManager(PIControllerManager* piManager) {
    m_piManager = piManager;
    m_ui->SetManagers(m_piManager, m_dataManager);

    // Get and set available devices
    if (m_piManager) {
      auto devices = m_piManager->GetDeviceNames();
      m_ui->SetAvailableDevices(devices);

      // Auto-select first device if available
      if (!devices.empty() && m_deviceName.empty()) {
        SetSelectedDevice(devices[0]);
      }
    }

    TryCreateScanner();
  }

  void SetSelectedDevice(const std::string& deviceName) {
    if (m_deviceName != deviceName) {
      Logger::GetInstance()->LogInfo("GridScannerManager: Changing device from " +
        m_deviceName + " to " + deviceName);

      m_deviceName = deviceName;
      m_ui->SetDeviceInfo(m_deviceName, m_dataChannel);

      // Reset scanner with new device
      if (m_scanner) {
        m_scanner.reset();
        m_motionAdapter.reset();
      }

      TryCreateScanner();
    }
  }

  void SetDataClient(DataClientManager* dataClient) {
    m_dataManager = dataClient;
    m_ui->SetManagers(m_piManager, m_dataManager);
    TryCreateScanner();
  }

  void UpdateHardwareConnections() {
    if (!m_motionAdapter || !m_motionAdapter->IsConnected()) {
      TryConnectMotionController();
    }
  }

  void Render() {
    if (m_ui) {
      m_ui->Render();
    }
  }

  void Show() {
    if (m_ui) m_ui->Show();
  }

  // FIX: Return raw pointer from unique_ptr using .get()
  GridVolumeScannerUI* GetUI() {
    return m_ui.get();  // This returns the raw pointer managed by unique_ptr
  }

  std::shared_ptr<GridVolumeScanner> GetScanner() {
    return m_scanner;
  }

  bool IsReady() const {
    return m_scanner &&
      m_scanner->IsMotionControllerConnected() &&
      m_scanner->IsDataChannelConnected();
  }

private:
  void TryCreateScanner() {
    if (m_dataManager && !m_scanner) {
      m_scanner = std::make_shared<GridVolumeScanner>(
        nullptr,  // No motion controller yet
        *m_dataManager,
        m_dataChannel
      );

      m_ui->SetScanner(m_scanner);

      Logger::GetInstance()->LogInfo(
        "GridVolumeScanner: Scanner created with data channel " + m_dataChannel);
    }

    if (m_scanner && m_piManager) {
      TryConnectMotionController();
    }
  }

  void TryConnectMotionController() {
    if (!m_piManager || !m_scanner) return;

    PIController* controller = m_piManager->GetController(m_deviceName);
    if (controller && controller->IsConnected()) {
      m_motionAdapter = std::make_shared<PIScanMotionAdapter>(
        controller, m_deviceName);
      m_scanner->SetMotionController(m_motionAdapter);

      Logger::GetInstance()->LogInfo(
        "GridVolumeScanner: Connected to " + m_deviceName);
    }
  }

  PIControllerManager* m_piManager;
  DataClientManager* m_dataManager;
  std::shared_ptr<GridVolumeScanner> m_scanner;
  std::unique_ptr<GridVolumeScannerUI> m_ui;
  std::shared_ptr<PIScanMotionAdapter> m_motionAdapter;
  std::string m_deviceName;
  std::string m_dataChannel;
};