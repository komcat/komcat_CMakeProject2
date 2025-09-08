// include/scanning/grid_scanner_manager.h
#pragma once
#include "grid_scanner.h"
#include "grid_scanner_ui.h"
#include "PIScanMotionAdapter.h"
#include "include/motions/pi_controller_manager.h"
#include "include/data/data_client_manager.h"

class GridScannerManager {
public:
  // Default constructor - no parameters needed
  GridScannerManager()
    : m_piManager(nullptr)
    , m_dataManager(nullptr)
    , m_deviceName("hex-left")
    , m_dataChannel("GPIB-Current") {

    // Create UI immediately (always visible)
    m_ui = std::make_unique<GridScannerUI>();
    m_ui->SetDeviceInfo(m_deviceName, m_dataChannel);
  }

  // Set managers after construction
  void SetPIManager(PIControllerManager* piManager) {
    m_piManager = piManager;
    m_ui->SetManagers(m_piManager, m_dataManager);
    TryCreateScanner();
  }

  void SetDataClient(DataClientManager* dataClient) {
    m_dataManager = dataClient;
    m_ui->SetManagers(m_piManager, m_dataManager);
    TryCreateScanner();
  }

  // Call this periodically to check for hardware
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
  GridScannerUI* GetUI() {
    return m_ui.get();  // This returns the raw pointer managed by unique_ptr
  }

  std::shared_ptr<GridScanner> GetScanner() {
    return m_scanner;
  }

private:
  void TryCreateScanner() {
    // Only create scanner if we have data manager and haven't created it yet
    if (m_dataManager && !m_scanner) {
      m_scanner = std::make_shared<GridScanner>(
        nullptr,  // No motion controller yet
        *m_dataManager,
        m_dataChannel
      );

      m_ui->SetScanner(m_scanner);

      Logger::GetInstance()->LogInfo(
        "GridScanner: Scanner created with data channel " + m_dataChannel);
    }

    // Try to connect motion controller if we have everything
    if (m_scanner && m_piManager) {
      TryConnectMotionController();
    }
  }

  void TryConnectMotionController() {
    if (!m_piManager || !m_scanner) return;

    PIController* controller = m_piManager->GetController(m_deviceName);
    if (controller && controller->IsConnected()) {
      // Create adapter and set it
      m_motionAdapter = std::make_shared<PIScanMotionAdapter>(
        controller, m_deviceName);
      m_scanner->SetMotionController(m_motionAdapter);

      Logger::GetInstance()->LogInfo(
        "GridScanner: Connected to " + m_deviceName);
    }
  }

  PIControllerManager* m_piManager;
  DataClientManager* m_dataManager;
  std::shared_ptr<GridScanner> m_scanner;
  std::unique_ptr<GridScannerUI> m_ui;
  std::shared_ptr<PIScanMotionAdapter> m_motionAdapter;
  std::string m_deviceName;
  std::string m_dataChannel;
};