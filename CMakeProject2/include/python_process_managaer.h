#include <iostream>
#include <string>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#endif

class PythonProcessManager {
private:
#ifdef _WIN32
  PROCESS_INFORMATION cld101xProcessInfo;
  PROCESS_INFORMATION keithleyProcessInfo;
#else
  pid_t cld101xPid;
  pid_t keithleyPid;
#endif

  bool cld101xRunning;
  bool keithleyRunning;
  Logger* m_logger;

public:
  PythonProcessManager() : cld101xRunning(false), keithleyRunning(false) {
    m_logger = Logger::GetInstance();
    m_logger->LogInfo("PythonProcessManager: Initialized");

#ifdef _WIN32
    ZeroMemory(&cld101xProcessInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&keithleyProcessInfo, sizeof(PROCESS_INFORMATION));
#endif
  }

  ~PythonProcessManager() {
    StopAllProcesses();
    m_logger->LogInfo("PythonProcessManager: Destroyed");
  }

  bool StartCLD101xServer() {
    if (cld101xRunning) {
      m_logger->LogWarning("PythonProcessManager: CLD101x server already running");
      return true;
    }

#ifdef _WIN32
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Hide the console window

    // Use the exact path from your screenshot
    std::string command = "python C:\\Windows-SSD\\SOFTWARE\\serverPython\\cld101x_server.py";

    if (CreateProcess(NULL, const_cast<LPSTR>(command.c_str()), NULL, NULL, FALSE,
      CREATE_NEW_CONSOLE, NULL, NULL, &si, &cld101xProcessInfo)) {
      cld101xRunning = true;
      m_logger->LogInfo("PythonProcessManager: Started CLD101x server");
      return true;
    }
    else {
      m_logger->LogError("PythonProcessManager: Failed to start CLD101x server - Error code: " +
        std::to_string(GetLastError()));
      return false;
    }
#else
    // Unix implementation
    cld101xPid = fork();

    if (cld101xPid < 0) {
      m_logger->LogError("PythonProcessManager: Failed to fork process for CLD101x server");
      return false;
    }
    else if (cld101xPid == 0) {
      // Child process
      if (execlp("python", "python", "/Windows-SSD/SOFTWARE/serverPython/cld101x_server.py", NULL) < 0) {
        exit(1);
      }
    }
    else {
      // Parent process
      cld101xRunning = true;
      m_logger->LogInfo("PythonProcessManager: Started CLD101x server with PID " +
        std::to_string(cld101xPid));
      return true;
    }
#endif
  }

  bool StartKeithleyScript() {
    if (keithleyRunning) {
      m_logger->LogWarning("PythonProcessManager: Keithley script already running");
      return true;
    }

#ifdef _WIN32
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Hide the console window

    // Use the exact path from your screenshot
    std::string command = "python C:\\Windows-SSD\\SOFTWARE\\serverPython\\keithley_1stch.py";

    if (CreateProcess(NULL, const_cast<LPSTR>(command.c_str()), NULL, NULL, FALSE,
      CREATE_NEW_CONSOLE, NULL, NULL, &si, &keithleyProcessInfo)) {
      keithleyRunning = true;
      m_logger->LogInfo("PythonProcessManager: Started Keithley script");
      return true;
    }
    else {
      m_logger->LogError("PythonProcessManager: Failed to start Keithley script - Error code: " +
        std::to_string(GetLastError()));
      return false;
    }
#else
    // Unix implementation
    keithleyPid = fork();

    if (keithleyPid < 0) {
      m_logger->LogError("PythonProcessManager: Failed to fork process for Keithley script");
      return false;
    }
    else if (keithleyPid == 0) {
      // Child process
      if (execlp("python", "python", "/Windows-SSD/SOFTWARE/serverPython/keithley_1stch.py", NULL) < 0) {
        exit(1);
      }
    }
    else {
      // Parent process
      keithleyRunning = true;
      m_logger->LogInfo("PythonProcessManager: Started Keithley script with PID " +
        std::to_string(keithleyPid));
      return true;
    }
#endif
  }

  void StopCLD101xServer() {
    if (!cld101xRunning) {
      return;
    }

#ifdef _WIN32
    if (TerminateProcess(cld101xProcessInfo.hProcess, 0)) {
      CloseHandle(cld101xProcessInfo.hProcess);
      CloseHandle(cld101xProcessInfo.hThread);
      ZeroMemory(&cld101xProcessInfo, sizeof(PROCESS_INFORMATION));
      cld101xRunning = false;
      m_logger->LogInfo("PythonProcessManager: Stopped CLD101x server");
    }
    else {
      m_logger->LogError("PythonProcessManager: Failed to stop CLD101x server - Error code: " +
        std::to_string(GetLastError()));
    }
#else
    if (kill(cld101xPid, SIGTERM) == 0) {
      cld101xRunning = false;
      m_logger->LogInfo("PythonProcessManager: Stopped CLD101x server");
    }
    else {
      m_logger->LogError("PythonProcessManager: Failed to stop CLD101x server - Error: " +
        std::string(strerror(errno)));
    }
#endif
  }

  void StopKeithleyScript() {
    if (!keithleyRunning) {
      return;
    }

#ifdef _WIN32
    if (TerminateProcess(keithleyProcessInfo.hProcess, 0)) {
      CloseHandle(keithleyProcessInfo.hProcess);
      CloseHandle(keithleyProcessInfo.hThread);
      ZeroMemory(&keithleyProcessInfo, sizeof(PROCESS_INFORMATION));
      keithleyRunning = false;
      m_logger->LogInfo("PythonProcessManager: Stopped Keithley script");
    }
    else {
      m_logger->LogError("PythonProcessManager: Failed to stop Keithley script - Error code: " +
        std::to_string(GetLastError()));
    }
#else
    if (kill(keithleyPid, SIGTERM) == 0) {
      keithleyRunning = false;
      m_logger->LogInfo("PythonProcessManager: Stopped Keithley script");
    }
    else {
      m_logger->LogError("PythonProcessManager: Failed to stop Keithley script - Error: " +
        std::string(strerror(errno)));
    }
#endif
  }

  void StopAllProcesses() {
    StopCLD101xServer();
    StopKeithleyScript();
  }

  bool IsScriptRunning(const std::string& name) {
    if (name == "cld101x") {
      return cld101xRunning;
    }
    else if (name == "keithley") {
      return keithleyRunning;
    }
    return false;
  }
};