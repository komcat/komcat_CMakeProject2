// script_editor_ui.cpp
#include "include/script/script_editor_ui.h"
#include "include/script/print_operation.h"
#include "imgui.h"
#include <fstream>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <ctime>

// In the constructor, include the print viewer reference
ScriptEditorUI::ScriptEditorUI(MachineOperations& machineOps, ScriptPrintViewer* printViewer)
  : m_isVisible(false),
  m_showCommandHelp(false),
  m_machineOps(machineOps),
  m_executor(machineOps),
  m_name("Script Editor"),
  m_scriptUIManager(std::make_unique<ScriptUIManager>()),
  m_printViewer(printViewer)
{


  // Initialize editor buffer
  memset(m_editorBuffer, 0, EDITOR_BUFFER_SIZE);

  // Set up executor callbacks
  m_executor.SetExecutionCallback([this](ScriptExecutor::ExecutionState state) {
    OnScriptExecutionStateChanged(state);
  });

  m_executor.SetLogCallback([this](const std::string& message) {
    OnScriptLog(message);
  });

  // Initialize command help
  InitializeCommandHelp();

  // Add some sample scripts
  AddSampleScript("Basic Movement",
    "# Basic movement script\n"
    "MOVE gantry-main TO node_4027 IN Process_Flow\n"
    "WAIT 1000\n"
    "MOVE hex-left TO node_5480 IN Process_Flow\n"
    "MOVE hex-right TO node_5136 IN Process_Flow\n");

  AddSampleScript("Lens Alignment",
    "# Lens alignment script\n"
    "# Move to starting position\n"
    "MOVE_TO_POINT hex-left approachlensgrip\n"
    "# Turn on laser for alignment\n"
    "LASER_ON\n"
    "SET_LASER_CURRENT 0.150\n"
    "# Run alignment scan\n"
    "RUN_SCAN hex-left GPIB-Current 0.0005,0.0002,0.0001\n"
    "WAIT 1000\n"
    "LASER_OFF\n");

  AddSampleScript("Flow Control Example",
    "# Example with variables and flow control\n"
    "SET $current = 0.150\n"
    "SET $count = 1\n\n"
    "# Turn on laser with variable current\n"
    "LASER_ON\n"
    "SET_LASER_CURRENT $current\n\n"
    "# Conditional execution\n"
    "IF $current > 0.1\n"
    "  PRINT Current is above threshold\n"
    "  WAIT 500\n"
    "ELSE\n"
    "  PRINT Current is below threshold\n"
    "  WAIT 1000\n"
    "ENDIF\n\n"
    "# Loop example\n"
    "WHILE $count <= 3\n"
    "  PRINT Loop iteration $count\n"
    "  SET $count = $count + 1\n"
    "  WAIT 500\n"
    "ENDWHILE\n\n"
    "# Turn off laser\n"
    "LASER_OFF\n");
  // Set up print handler for the executor
  if (m_printViewer) {
    m_executor.SetPrintHandler([this](const std::string& message) {
      m_printViewer->AddPrintMessage(message);
    });

    // Also set the global handler for direct operations
    PrintOperation::SetPrintHandler([this](const std::string& message) {
      if (m_printViewer) {
        m_printViewer->AddPrintMessage(message);
      }
    });
  }

  // Set UI manager for the executor
  m_executor.SetUIManager(m_scriptUIManager.get());

  m_executor.SetUIManager(m_scriptUIManager.get());
}

ScriptEditorUI::~ScriptEditorUI() {
  // Make sure execution stops when UI is destroyed
  m_executor.Stop();
}

std::string ScriptEditorUI::TrimString(const std::string& str) {
  if (str.empty()) {
    return str;
  }

  // Find first non-whitespace character
  size_t start = str.find_first_not_of(" \t\r\n");

  // If the string is all whitespace
  if (start == std::string::npos) {
    return "";
  }

  // Find last non-whitespace character
  size_t end = str.find_last_not_of(" \t\r\n");

  // Return the trimmed substring
  return str.substr(start, end - start + 1);
}

void ScriptEditorUI::RenderUI() {
  if (!m_isVisible) {
    return;
  }

  // Main editor window
  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar;
  ImGui::Begin("Machine Script Editor", &m_isVisible, windowFlags);

  // Menu bar
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New")) {
        memset(m_editorBuffer, 0, EDITOR_BUFFER_SIZE);
        m_script.clear();
        strcpy_s(m_editorBuffer, sizeof(m_editorBuffer), "# New script");
      }
      if (ImGui::MenuItem("Open...")) {
        ShowOpenDialog();
      }
      if (ImGui::MenuItem("Save")) {
        if (m_currentFilePath.empty()) {
          ShowSaveDialog();
        }
        else {
          SaveScript(m_currentFilePath);
        }
      }
      if (ImGui::MenuItem("Save As...")) {
        ShowSaveDialog();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit")) {
        m_isVisible = false;
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Recent Files")) {
      if (m_recentFiles.empty()) {
        ImGui::MenuItem("(No recent files)", nullptr, false, false);
      }
      else {
        for (const auto& file : m_recentFiles) {
          if (ImGui::MenuItem(file.c_str())) {
            if (LoadScript(file)) {
              m_currentFilePath = file;
            }
          }
        }
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Examples")) {
      for (const auto& [name, script] : m_sampleScripts) {
        if (ImGui::MenuItem(name.c_str())) {
          SetScript(script);
        }
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Command Help", nullptr, &m_showCommandHelp);
      ImGui::Separator();

      // Font size menu
      if (ImGui::BeginMenu("Font Size")) {
        if (ImGui::MenuItem("Default", nullptr, m_fontSize == 1.0f)) {
          m_fontSize = 1.0f;
        }
        if (ImGui::MenuItem("Small", nullptr, m_fontSize == 0.85f)) {
          m_fontSize = 0.85f;
        }
        if (ImGui::MenuItem("Large", nullptr, m_fontSize == 2.0f)) {
          m_fontSize = 2.0f;
        }
        if (ImGui::MenuItem("Extra Large", nullptr, m_fontSize == 4.0f)) {
          m_fontSize = 4.0f;
        }
        ImGui::Separator();
        // Custom font size slider
        ImGui::SliderFloat("Custom", &m_fontSize, 0.5f, 2.0f, "%.2f");
        ImGui::EndMenu();
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("Command Reference", nullptr, &m_showCommandHelp)) {
        // Toggle command help panel
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("Command Reference", nullptr, &m_showCommandHelp)) {
        // Toggle command help panel
      }
      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  // Split the window into editor and output/help panels
  float windowWidth = ImGui::GetContentRegionAvail().x;
  float windowHeight = ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - 10.0f;

  // Create a layout with editor on the left, controls on top right, and log on bottom right
  float editorWidth = windowWidth * 0.6f;
  float controlsHeight = windowHeight * 0.3f;
  float logHeight = windowHeight - controlsHeight;

  // Left panel - editor
  ImGui::BeginChild("EditorPanel", ImVec2(editorWidth, windowHeight), true);
  RenderEditorSection();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right panel divided into controls and log
  ImGui::BeginGroup();

  // Controls panel
  ImGui::BeginChild("ControlsPanel", ImVec2(windowWidth - editorWidth - 10.0f, controlsHeight), true);
  RenderControlSection();
  ImGui::EndChild();

  // Log panel
  ImGui::BeginChild("LogPanel", ImVec2(windowWidth - editorWidth - 10.0f, logHeight), true);
  RenderLogSection();
  ImGui::EndChild();

  ImGui::EndGroup();

  // Command help window (separate window or panel)
  if (m_showCommandHelp) {
    RenderCommandHelpSection();
  }
  // Render file dialog if open
  RenderFileDialog();
  ImGui::End();
}

void ScriptEditorUI::RenderEditorSection() {
  // Ensure editor buffer has content from m_script
  if (strlen(m_editorBuffer) == 0 && !m_script.empty()) {
    strncpy_s(m_editorBuffer, EDITOR_BUFFER_SIZE, m_script.c_str(), EDITOR_BUFFER_SIZE - 1);
    m_editorBuffer[EDITOR_BUFFER_SIZE - 1] = '\0';
  }

  // Editor title and controls
  ImGui::Text("Script Editor");
  ImGui::Separator();

  // Text editor with syntax highlighting would be ideal here
  // For simplicity, we'll use a basic input text area
  ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;

  // Disable editing during execution
  if (m_executor.GetState() == ScriptExecutor::ExecutionState::Running ||
    m_executor.GetState() == ScriptExecutor::ExecutionState::Paused) {
    flags |= ImGuiInputTextFlags_ReadOnly;
  }

  // Apply font scaling
  ImFont* font = ImGui::GetFont();
  float oldScale = font->Scale;
  font->Scale *= m_fontSize;
  ImGui::PushFont(font);

  // Multiline text editor
  if (ImGui::InputTextMultiline("##editor", m_editorBuffer, EDITOR_BUFFER_SIZE,
    ImVec2(-1, -1), flags)) {
    // Update script when text changes
    m_script = m_editorBuffer;
  }

  // Restore font scale
  font->Scale = oldScale;
  ImGui::PopFont();
}

void ScriptEditorUI::RenderControlSection() {
  // Title
  ImGui::Text("Script Controls");
  ImGui::Separator();

  // Execution controls
  ScriptExecutor::ExecutionState state = m_executor.GetState();


 // Execute/Stop button
  if (state == ScriptExecutor::ExecutionState::Idle ||
    state == ScriptExecutor::ExecutionState::Completed ||
    state == ScriptExecutor::ExecutionState::Error) {
    if (ImGui::Button("Execute Script", ImVec2(150, 0))) {
      // Make sure the previous execution is completely finished
      m_executor.Stop(); // Ensure clean state

      // Small delay to ensure thread cleanup
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

      // When starting execution, update the script from the buffer
      m_script = m_editorBuffer;
      if (!m_executor.ExecuteScript(m_script)) {
        m_statusMessage = "Failed to start script execution";
        m_statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      }
    }
  }
  else {
    if (ImGui::Button("Stop Execution", ImVec2(150, 0))) {
      m_executor.Stop();
      m_statusMessage = "Stopping script execution...";
      m_statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    }
  }

  // Pause/Resume button (only during execution)
  if (state == ScriptExecutor::ExecutionState::Running) {
    ImGui::SameLine();
    if (ImGui::Button("Pause", ImVec2(80, 0))) {
      m_executor.Pause();
    }
  }
  else if (state == ScriptExecutor::ExecutionState::Paused) {
    ImGui::SameLine();
    if (ImGui::Button("Resume", ImVec2(80, 0))) {
      m_executor.Resume();
    }
  }

  // Status display
  ImGui::Separator();
  // Add auto-confirm checkbox
  if (ImGui::Checkbox("Auto-confirm Prompts", &m_autoConfirmPrompts)) {
    m_scriptUIManager->SetAutoConfirm(m_autoConfirmPrompts);
  }

  // Add confirmation UI if script is running and waiting
  if (m_executor.GetState() == ScriptExecutor::ExecutionState::Running ||
    m_executor.GetState() == ScriptExecutor::ExecutionState::Paused) {

    if (m_scriptUIManager->IsWaitingForConfirmation() && !m_autoConfirmPrompts) {
      ImGui::Separator();
      ImGui::TextWrapped("Script requires confirmation:");

      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
      ImGui::TextWrapped("%s", m_scriptUIManager->GetLastMessage().c_str());
      ImGui::PopStyleColor();

      if (ImGui::Button("Confirm", ImVec2(100, 0))) {
        m_scriptUIManager->ConfirmationReceived(true);
      }

      ImGui::SameLine();

      if (ImGui::Button("Cancel", ImVec2(100, 0))) {
        m_scriptUIManager->ConfirmationReceived(false);
      }
    }
  }

  // Status display
  ImGui::Separator();
  // Status text with color based on state
  ImVec4 statusColor;
  const char* statusText = nullptr;

  switch (state) {
  case ScriptExecutor::ExecutionState::Idle:
    statusColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    statusText = "Ready";
    break;
  case ScriptExecutor::ExecutionState::Running:
    statusColor = ImVec4(0.0f, 0.7f, 0.0f, 1.0f);
    statusText = "Running";
    break;
  case ScriptExecutor::ExecutionState::Paused:
    statusColor = ImVec4(0.9f, 0.7f, 0.0f, 1.0f);
    statusText = "Paused";
    break;
  case ScriptExecutor::ExecutionState::Completed:
    statusColor = ImVec4(0.0f, 0.7f, 0.7f, 1.0f);
    statusText = "Completed";
    break;
  case ScriptExecutor::ExecutionState::Error:
    statusColor = ImVec4(0.9f, 0.0f, 0.0f, 1.0f);
    statusText = "Error";
    break;
  }

  ImGui::TextColored(statusColor, "Status: %s", statusText);

  // Current operation (if running)
  if (state == ScriptExecutor::ExecutionState::Running ||
    state == ScriptExecutor::ExecutionState::Paused) {
    ImGui::Text("Current: %s", m_executor.GetCurrentOperation().c_str());

    // Progress bar
    float progress = m_executor.GetProgress();
    ImGui::ProgressBar(progress, ImVec2(-1, 0));
    ImGui::Text("Line %d of %d", m_executor.GetCurrentLine(), m_executor.GetTotalLines());
  }

  // Status message (if any)
  if (!m_statusMessage.empty()) {
    auto now = std::chrono::steady_clock::now();
    if (now < m_statusExpiry) {
      ImGui::TextWrapped("%s", m_statusMessage.c_str());
    }
    else {
      m_statusMessage.clear();
    }
  }
}

void ScriptEditorUI::RenderLogSection() {
  ImGui::Text("Execution Log");
  ImGui::Separator();

  // Log display with auto-scroll
  ImGui::BeginChild("LogText", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true, ImGuiWindowFlags_HorizontalScrollbar);

  const auto& log = m_executor.GetLog();
  for (const auto& entry : log) {
    // Color error messages in red
    if (entry.substr(0, 6) == "ERROR:") {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", entry.c_str());
    }
    else {
      ImGui::TextUnformatted(entry.c_str());
    }
  }

  // Auto-scroll to bottom
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10) {
    ImGui::SetScrollHereY(1.0f);
  }

  ImGui::EndChild();

  // Clear log button
  if (ImGui::Button("Clear Log")) {
    // In a real app, you'd clear the log here
  }
}


void ScriptEditorUI::RenderCommandHelpSection() {
  ImGui::Begin("Command Reference", &m_showCommandHelp);

  // Command categories
  static const char* categories[] = {
      "Motion Commands", "I/O Commands", "Pneumatic Commands",
      "Laser Commands", "Scanning Commands", "Utility Commands",
      "Flow Control"
  };

  static int currentCategory = 0;
  ImGui::BeginChild("Categories", ImVec2(150, 0), true);
  for (int i = 0; i < IM_ARRAYSIZE(categories); i++) {
    if (ImGui::Selectable(categories[i], currentCategory == i)) {
      currentCategory = i;
    }
  }
  ImGui::EndChild();

  ImGui::SameLine();

  // Command details
  ImGui::BeginChild("CommandDetails", ImVec2(0, 0), true);

  // Filter commands by category
  std::vector<std::string> categoryCommands;
  for (const auto& [cmd, help] : m_commandHelp) {
    // Assign each command to its category (simplified)
    bool shouldShow = false;
    switch (currentCategory) {
    case 0: // Motion Commands
      shouldShow = (cmd.find("MOVE") != std::string::npos);
      break;
    case 1: // I/O Commands
      shouldShow = (cmd.find("OUTPUT") != std::string::npos ||
        cmd.find("INPUT") != std::string::npos);
      break;
    case 2: // Pneumatic Commands
      shouldShow = (cmd.find("SLIDE") != std::string::npos);
      break;
    case 3: // Laser Commands
      shouldShow = (cmd.find("LASER") != std::string::npos ||
        cmd.find("TEC") != std::string::npos);
      break;
    case 4: // Scanning Commands
      shouldShow = (cmd.find("SCAN") != std::string::npos);
      break;
    case 5: // Utility Commands
      shouldShow = (cmd == "WAIT" || cmd == "PROMPT" || cmd == "PRINT" ||
        cmd == "SET");
      break;
    case 6: // Flow Control
      shouldShow = (cmd == "IF" || cmd == "ELSE" || cmd == "ENDIF" ||
        cmd == "FOR" || cmd == "ENDFOR" ||
        cmd == "WHILE" || cmd == "ENDWHILE");
      break;
    }

    if (shouldShow) {
      categoryCommands.push_back(cmd);
    }
  }

  // Display commands for this category
  for (const auto& cmd : categoryCommands) {
    const auto& help = m_commandHelp[cmd];

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.7f, 0.9f));
    if (ImGui::CollapsingHeader(cmd.c_str())) {
      ImGui::Indent(10.0f);

      ImGui::TextWrapped("Syntax: %s", help.syntax.c_str());
      ImGui::Spacing();

      ImGui::TextWrapped("%s", help.description.c_str());
      ImGui::Spacing();

      ImGui::Text("Example:");
      ImGui::Indent(10.0f);
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
      ImGui::TextWrapped("%s", help.example.c_str());
      ImGui::PopStyleColor();
      ImGui::Unindent(10.0f);

      // Add a button to insert the example
      if (ImGui::Button("Insert Example")) {
        // Append the example to the current script
        std::string newText = m_editorBuffer;
        if (!newText.empty() && newText.back() != '\n') {
          newText += "\n";
        }
        newText += help.example;
        if (newText.length() < EDITOR_BUFFER_SIZE - 1) {
          strcpy_s(m_editorBuffer, sizeof(m_editorBuffer), newText.c_str());
          m_script = newText;
        }
      }

      ImGui::Unindent(10.0f);
    }
    ImGui::PopStyleColor();
  }

  ImGui::EndChild();

  ImGui::End();
}

void ScriptEditorUI::InitializeCommandHelp() {
  // Motion commands
  m_commandHelp["MOVE"] = {
      "MOVE <device> TO <node> IN <graph>",
      "Moves a device to a specific node within a graph.",
      "MOVE gantry-main TO node_4027 IN Process_Flow"
  };

  m_commandHelp["MOVE_TO_POINT"] = {
      "MOVE_TO_POINT <device> <position>",
      "Moves a device to a named position defined in the configuration.",
      "MOVE_TO_POINT hex-left approachlensgrip"
  };

  m_commandHelp["MOVE_RELATIVE"] = {
      "MOVE_RELATIVE <device> <axis> <distance>",
      "Moves a device by a relative distance along the specified axis.",
      "MOVE_RELATIVE hex-left Z -0.01"
  };

  // I/O commands
  m_commandHelp["SET_OUTPUT"] = {
      "SET_OUTPUT <device> <pin> <ON|OFF> [delay_ms]",
      "Sets a digital output pin to the specified state, with an optional delay after setting.",
      "SET_OUTPUT IOBottom 0 ON 200"
  };

  m_commandHelp["READ_INPUT"] = {
      "READ_INPUT <device> <pin> $variable",
      "Reads the state of a digital input pin and stores it in a variable.",
      "READ_INPUT IOBottom 5 $sensorState"
  };

  // Pneumatic commands
  m_commandHelp["EXTEND_SLIDE"] = {
      "EXTEND_SLIDE <slide_name>",
      "Extends the specified pneumatic slide.",
      "EXTEND_SLIDE UV_Head"
  };

  m_commandHelp["RETRACT_SLIDE"] = {
      "RETRACT_SLIDE <slide_name>",
      "Retracts the specified pneumatic slide.",
      "RETRACT_SLIDE Dispenser_Head"
  };

  // Laser commands
  m_commandHelp["LASER_ON"] = {
      "LASER_ON [laser_name]",
      "Turns on the laser. If laser_name is omitted, the default laser is used.",
      "LASER_ON"
  };

  m_commandHelp["LASER_OFF"] = {
      "LASER_OFF [laser_name]",
      "Turns off the laser. If laser_name is omitted, the default laser is used.",
      "LASER_OFF"
  };

  m_commandHelp["SET_LASER_CURRENT"] = {
      "SET_LASER_CURRENT <current> [laser_name]",
      "Sets the laser current to the specified value in amperes.",
      "SET_LASER_CURRENT 0.150"
  };

  m_commandHelp["TEC_ON"] = {
      "TEC_ON [laser_name]",
      "Turns on the TEC (Thermoelectric Cooler) for temperature control.",
      "TEC_ON"
  };

  m_commandHelp["TEC_OFF"] = {
      "TEC_OFF [laser_name]",
      "Turns off the TEC (Thermoelectric Cooler).",
      "TEC_OFF"
  };

  m_commandHelp["SET_TEC_TEMPERATURE"] = {
      "SET_TEC_TEMPERATURE <temperature> [laser_name]",
      "Sets the TEC target temperature in degrees Celsius.",
      "SET_TEC_TEMPERATURE 25.0"
  };

  m_commandHelp["WAIT_FOR_TEMPERATURE"] = {
      "WAIT_FOR_TEMPERATURE <temp> [tolerance=0.5] [timeout_ms=30000] [laser_name]",
      "Waits for the laser temperature to stabilize at the specified value.",
      "WAIT_FOR_TEMPERATURE 25.0 0.5 10000"
  };

  // Scanning commands
  m_commandHelp["RUN_SCAN"] = {
      "RUN_SCAN <device> <channel> <step_sizes> [settling_time=300] [axes=Z,X,Y]",
      "Runs an optimization scan on the specified device, using the data channel for feedback.",
      "RUN_SCAN hex-left GPIB-Current 0.0005,0.0002,0.0001"
  };

  // Utility commands
  m_commandHelp["WAIT"] = {
      "WAIT <milliseconds>",
      "Pauses script execution for the specified time in milliseconds.",
      "WAIT 1000"
  };

  m_commandHelp["PROMPT"] = {
      "PROMPT <message>",
      "Displays a message to the user and waits for confirmation.",
      "PROMPT Please check alignment before continuing"
  };

  m_commandHelp["PRINT"] = {
      "PRINT <message>",
      "Displays a message in the log without pausing execution.",
      "PRINT Starting alignment procedure"
  };

  m_commandHelp["SET"] = {
      "SET $variable = <expression>",
      "Assigns a value to a variable for use in the script.",
      "SET $current = 0.150"
  };

  // Flow control commands
  m_commandHelp["IF"] = {
      "IF <condition>\n  ...\n[ELSE\n  ...]\nENDIF",
      "Conditionally executes a block of code based on the specified condition.",
      "IF $current > 0.1\n  PRINT Current is above threshold\nELSE\n  PRINT Current is below threshold\nENDIF"
  };

  m_commandHelp["FOR"] = {
      "FOR $variable = <start> TO <end> [STEP <step>]\n  ...\nENDFOR",
      "Executes a block of code repeatedly, with the variable incrementing from start to end.",
      "FOR $i = 1 TO 5\n  PRINT Iteration number $i\n  WAIT 500\nENDFOR"
  };

  m_commandHelp["WHILE"] = {
      "WHILE <condition>\n  ...\nENDWHILE",
      "Executes a block of code repeatedly as long as the condition is true.",
      "SET $count = 0\nWHILE $count < 5\n  PRINT Count is $count\n  SET $count = $count + 1\nENDWHILE"
  };
}

void ScriptEditorUI::OnScriptExecutionStateChanged(ScriptExecutor::ExecutionState state) {
  // Update UI status based on execution state
  switch (state) {
  case ScriptExecutor::ExecutionState::Completed:
    m_statusMessage = "Script execution completed successfully";
    break;
  case ScriptExecutor::ExecutionState::Error:
    m_statusMessage = "Error during script execution";
    break;
  case ScriptExecutor::ExecutionState::Paused:
    m_statusMessage = "Script execution paused";
    break;
  default:
    break;
  }

  // Set status message expiry time
  m_statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(5);
}

void ScriptEditorUI::OnScriptLog(const std::string& message) {
  // We don't need to do anything here since the log is stored in the executor
  // and we read it directly in RenderLogSection
}

bool ScriptEditorUI::LoadScript(const std::string& filename) {
  try {
    // Ensure .aas extension
    std::string actualFilename = filename;
    if (actualFilename.find(".aas") == std::string::npos) {
      actualFilename += ".aas";
    }

    // Create directory if needed
    std::filesystem::path filePath(actualFilename);
    std::filesystem::path dir = filePath.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
      std::filesystem::create_directories(dir);
    }

    std::ifstream file(actualFilename);
    if (!file.is_open()) {
      m_statusMessage = "Error: Could not open file " + actualFilename;
      m_statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
      std::istreambuf_iterator<char>());

    SetScript(content);
    AddToRecentFiles(actualFilename);
    m_statusMessage = "Script loaded from " + actualFilename;
    m_statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    return true;
  }
  catch (const std::exception& e) {
    m_statusMessage = std::string("Error loading script: ") + e.what();
    m_statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    return false;
  }
}

bool ScriptEditorUI::SaveScript(const std::string& filename) {
  try {
    // Ensure .aas extension
    std::string actualFilename = filename;
    if (actualFilename.find(".aas") == std::string::npos) {
      actualFilename += ".aas";
    }

    // Create directory if it doesn't exist
    std::filesystem::path filePath(actualFilename);
    std::filesystem::path dir = filePath.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
      std::filesystem::create_directories(dir);
    }

    std::ofstream file(actualFilename);
    if (!file.is_open()) {
      m_statusMessage = "Error: Could not open file " + actualFilename + " for writing";
      m_statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      return false;
    }

    file << m_script;

    m_statusMessage = "Script saved to " + actualFilename;
    m_statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    return true;
  }
  catch (const std::exception& e) {
    m_statusMessage = std::string("Error saving script: ") + e.what();
    m_statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    return false;
  }
}

void ScriptEditorUI::SetScript(const std::string& script) {
  m_script = script;

  // Update editor buffer
  strncpy_s(m_editorBuffer, EDITOR_BUFFER_SIZE, script.c_str(), EDITOR_BUFFER_SIZE - 1);
  m_editorBuffer[EDITOR_BUFFER_SIZE - 1] = '\0';
}

void ScriptEditorUI::AddSampleScript(const std::string& name, const std::string& script) {
  m_sampleScripts[name] = script;
}

void ScriptEditorUI::ShowOpenDialog() {
  m_showFileDialog = true;
  m_isOpenDialog = true;
  strcpy_s(m_filePathBuffer, sizeof(m_filePathBuffer), "scripts/");
}

void ScriptEditorUI::ShowSaveDialog() {
  m_showFileDialog = true;
  m_isOpenDialog = false;
  if (m_currentFilePath.empty()) {
    strcpy_s(m_filePathBuffer, sizeof(m_filePathBuffer), "scripts/new_script.aas");
  }
  else {
    strcpy_s(m_filePathBuffer, sizeof(m_filePathBuffer), "scripts/new_script.aas");
  }
}

// Enhanced RenderFileDialog method for script_editor_ui.cpp
void ScriptEditorUI::RenderFileDialog() {
  if (!m_showFileDialog) return;

  // Set a larger window size for the file browser
  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

  ImGui::Begin("File Dialog", &m_showFileDialog);

  ImGui::Text(m_isOpenDialog ? "Open Script File" : "Save Script File");
  ImGui::Separator();

  // Current directory display and navigation
  static std::string currentDir = "scripts/";

  // Ensure the scripts directory exists
  std::filesystem::path scriptsPath(currentDir);
  if (!std::filesystem::exists(scriptsPath)) {
    std::filesystem::create_directories(scriptsPath);
  }

  ImGui::Text("Directory: %s", currentDir.c_str());

  // Directory navigation buttons
  if (ImGui::Button("Up")) {
    std::filesystem::path path(currentDir);
    if (path.has_parent_path() && path != path.root_path()) {
      currentDir = path.parent_path().string() + "/";
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Scripts Folder")) {
    currentDir = "scripts/";
  }

  ImGui::SameLine();
  if (ImGui::Button("Refresh")) {
    // Force refresh of file list (will happen automatically)
  }

  ImGui::Separator();

  // File browser section
  ImGui::BeginChild("FileBrowser", ImVec2(-1, -80), true);

  try {
    std::vector<std::filesystem::directory_entry> directories;
    std::vector<std::filesystem::directory_entry> aasFiles;

    // Scan the current directory
    if (std::filesystem::exists(currentDir)) {
      for (const auto& entry : std::filesystem::directory_iterator(currentDir)) {
        if (entry.is_directory()) {
          directories.push_back(entry);
        }
        else if (entry.is_regular_file()) {
          std::string extension = entry.path().extension().string();
          std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
          if (extension == ".aas") {
            aasFiles.push_back(entry);
          }
        }
      }
    }

    // Sort directories and files
    std::sort(directories.begin(), directories.end(),
      [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
      return a.path().filename() < b.path().filename();
    });

    std::sort(aasFiles.begin(), aasFiles.end(),
      [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
      return a.path().filename() < b.path().filename();
    });

    // Display directories first
    for (const auto& dir : directories) {
      std::string dirName = "[DIR] " + dir.path().filename().string();
      if (ImGui::Selectable(dirName.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
        if (ImGui::IsMouseDoubleClicked(0)) {
          currentDir = dir.path().string() + "/";
        }
      }
    }

    // Display .aas files
    for (const auto& file : aasFiles) {
      std::string fileName = file.path().filename().string();
      bool isSelected = (fileName == std::filesystem::path(m_filePathBuffer).filename().string());

      if (ImGui::Selectable(fileName.c_str(), isSelected)) {
        // When a file is selected, update the file path buffer
        std::string fullPath = file.path().string();
        strncpy_s(m_filePathBuffer, sizeof(m_filePathBuffer), fullPath.c_str(), sizeof(m_filePathBuffer) - 1);
        m_filePathBuffer[sizeof(m_filePathBuffer) - 1] = '\0';
      }

      // Double-click to open (for open dialog)
      if (m_isOpenDialog && ImGui::IsMouseDoubleClicked(0) &&
        ImGui::IsItemHovered()) {
        std::string fullPath = file.path().string();
        if (LoadScript(fullPath)) {
          m_currentFilePath = fullPath;
          m_showFileDialog = false;
        }
      }

      // Show file info on hover
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        try {
          auto fileTime = std::filesystem::last_write_time(file.path());
          auto fileSize = std::filesystem::file_size(file.path());

          // Convert file time to readable format
          auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
          auto time_t = std::chrono::system_clock::to_time_t(sctp);

          ImGui::Text("Size: %zu bytes", fileSize);
          char timeBuffer[26];  // ctime_s requires a 26-character buffer
          if (ctime_s(timeBuffer, sizeof(timeBuffer), &time_t) == 0) {
            ImGui::Text("Modified: %s", timeBuffer);
          }
          else {
            ImGui::Text("Modified: [Invalid time]");
          }
        }
        catch (...) {
          ImGui::Text("File information unavailable");
        }
        ImGui::EndTooltip();
      }
    }

    // Show message if no files found
    if (aasFiles.empty() && directories.empty()) {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No .aas files found in this directory");
    }
    else if (aasFiles.empty()) {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No .aas files found (only subdirectories)");
    }

  }
  catch (const std::filesystem::filesystem_error& e) {
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error reading directory: %s", e.what());
  }

  ImGui::EndChild();

  ImGui::Separator();

  // File path input
  ImGui::Text("File Path:");
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputText("##FilePath", m_filePathBuffer, sizeof(m_filePathBuffer))) {
    // Auto-add .aas extension if not present
    std::string filePath(m_filePathBuffer);
    if (!filePath.empty() && filePath.find(".aas") == std::string::npos &&
      filePath.find(".") == std::string::npos) {
      filePath += ".aas";
      strcpy_s(m_filePathBuffer, sizeof(m_filePathBuffer), filePath.c_str());
      m_filePathBuffer[sizeof(m_filePathBuffer) - 1] = '\0';
    }
  }

  ImGui::Separator();

  // Action buttons
  std::string actionText = m_isOpenDialog ? "Open" : "Save";
  if (ImGui::Button(actionText.c_str(), ImVec2(120, 0))) {
    std::string filePath(m_filePathBuffer);

    // If no path specified, show error
    if (filePath.empty()) {
      m_statusMessage = "Please select or enter a file name";
      m_statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    }
    else {
      // Ensure .aas extension
      if (filePath.find(".aas") == std::string::npos && filePath.find(".") == std::string::npos) {
        filePath += ".aas";
      }

      if (m_isOpenDialog) {
        if (LoadScript(filePath)) {
          m_currentFilePath = filePath;
          m_showFileDialog = false;
        }
      }
      else {
        if (SaveScript(filePath)) {
          m_currentFilePath = filePath;
          m_showFileDialog = false;
        }
      }
    }
  }

  ImGui::SameLine();

  if (ImGui::Button("Cancel", ImVec2(120, 0))) {
    m_showFileDialog = false;
  }

  // Quick access buttons for common locations
  ImGui::SameLine();
  if (ImGui::Button("New File")) {
    // Generate a new filename
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    struct tm timeinfo;
    if (localtime_s(&timeinfo, &time) == 0) {
      ss << currentDir << "script_" << std::put_time(&timeinfo, "%Y%m%d_%H%M%S") << ".aas";
    }
    else {
      ss << currentDir << "script_" << "default" << ".aas";  // fallback name
    }
    std::string newFile = ss.str();
    strcpy_s(m_filePathBuffer, sizeof(m_filePathBuffer), newFile.c_str());
    m_filePathBuffer[sizeof(m_filePathBuffer) - 1] = '\0';
  }

  ImGui::End();
}



void ScriptEditorUI::AddToRecentFiles(const std::string& filepath) {
  // Remove if already in list
  m_recentFiles.erase(
    std::remove(m_recentFiles.begin(), m_recentFiles.end(), filepath),
    m_recentFiles.end()
  );

  // Add to front
  m_recentFiles.insert(m_recentFiles.begin(), filepath);

  // Limit size
  if (m_recentFiles.size() > MAX_RECENT_FILES) {
    m_recentFiles.resize(MAX_RECENT_FILES);
  }
}