// DUTOperations.cpp

// CRITICAL: Include Windows/Winsock headers FIRST to prevent conflicts
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

// NOW include other headers
#include "AppContext.h"  // Safe to include after winsock headers
#include "include/data/DUTDataRecorder.h"
#include "SequenceStep.h"
#include "include/logger.h"

bool DUTStartRecordingOperation::Execute(MachineOperations& ops) {
  AppContext* context = &AppContext::GetInstance();  // Note the & to get address
  DUTDataRecorder* recorder = context->GetDUTDataRecorder();

  if (!recorder) {
    ops.LogError("DUTDataRecorder not available in AppContext");
    return false;
  }

  recorder->Start(m_serialNumber);
  ops.LogInfo("Started DUT recording for: " + m_serialNumber);
  return true;
}

bool DUTRecordDataOperation::Execute(MachineOperations& ops) {
  AppContext* context = &AppContext::GetInstance();  // Note the & to get address
  DUTDataRecorder* recorder = context->GetDUTDataRecorder();

  if (!recorder || !recorder->IsRecording()) {
    ops.LogWarning("DUT not recording - skipping data point");
    return true; // Non-fatal
  }

  // Get value from GlobalDataStore
  double value = 0.0;
  if (ops.GetDataValue(m_dataKey, value)) {
    recorder->AddDataPoint(m_dataKey, value);
    ops.LogInfo("Recorded DUT data: " + m_dataKey + " = " + std::to_string(value));
  }

  return true;
}

bool DUTEndRecordingOperation::Execute(MachineOperations& ops) {
  AppContext* context = &AppContext::GetInstance();  // Note the & to get address
  DUTDataRecorder* recorder = context->GetDUTDataRecorder();

  if (!recorder) {
    ops.LogError("DUTDataRecorder not available");
    return false;
  }

  recorder->End();

  if (m_exportCsv) {
    recorder->ExportToCSV();
  }
  if (m_exportJson) {
    recorder->ExportToJSON();
  }

  ops.LogInfo("Ended DUT recording with " +
    std::to_string(recorder->GetDataPointCount()) + " data points");
  return true;
}