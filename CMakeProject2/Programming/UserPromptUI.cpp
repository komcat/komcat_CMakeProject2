// UserPromptUI.cpp
#include "UserPromptUI.h"
#include <algorithm>
#include <chrono>
#include <cstring>  // for strncpy

UserPromptUI::UserPromptUI()
  : m_isVisible(false)
  , m_isPromptActive(false)
  , m_promptRequested(false)
  , m_title("User Confirmation")
  , m_message("")
  , m_result(PromptResult::PENDING)
  , m_autoConfirm(false)
  , m_autoConfirmDelay(3.0f)
  , m_promptStartTime(0.0f)
  , m_autoConfirmTriggered(false)
  , m_windowSizeCalculated(false)
  , m_calculatedWindowSize(ImVec2(500, 300))
  , m_showInputPrompt(false)
  , m_textInputRequested(false)
{
}

UserPromptUI::~UserPromptUI() {
}

float UserPromptUI::GetCurrentTime() const {
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration<float>(duration).count();
}

void UserPromptUI::RequestPrompt(const std::string& title, const std::string& message,
  std::function<void(PromptResult)> callback) {

  printf("[DEBUG] RequestPrompt called: title='%s', message='%s', auto-confirm=%s\n",
    title.c_str(), message.c_str(), m_autoConfirm ? "enabled" : "disabled");

  std::lock_guard<std::mutex> lock(m_mutex);

  m_title = title;
  m_message = message;
  m_callback = callback;
  m_result = PromptResult::PENDING;
  m_promptRequested = true;
  m_isVisible = true;

  m_windowSizeCalculated = false;
  m_autoConfirmTriggered = false;
  m_promptStartTime = GetCurrentTime();

  printf("[DEBUG] RequestPrompt: Auto-confirm will trigger in %.1f seconds\n", m_autoConfirmDelay);
}

void UserPromptUI::ShowPrompt(const std::string& title, const std::string& message,
  std::function<void(PromptResult)> callback) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_title = title;
  m_message = message;
  m_callback = callback;
  m_result = PromptResult::PENDING;
  m_isPromptActive = true;
  m_isVisible = true;

  m_windowSizeCalculated = false;
  m_autoConfirmTriggered = false;
  m_promptStartTime = GetCurrentTime();

  ImGui::OpenPopup(m_title.c_str());
}

void UserPromptUI::CalculateWindowSize() {
  if (m_windowSizeCalculated) {
    return;
  }

  ImVec2 minSize = ImVec2(500, 300);
  ImVec2 maxSize = ImVec2(800, 600);

  float estimatedHeight = 120;

  if (!m_title.empty() && m_title != "User Confirmation") {
    estimatedHeight += 30;
  }

  float messageLines = (float)m_message.length() / 80.0f;
  if (messageLines < 1.0f) messageLines = 1.0f;
  estimatedHeight += messageLines * 20.0f + 40;

  if (m_autoConfirm) {
    estimatedHeight += 60;
  }

  estimatedHeight = (std::max)(minSize.y, (std::min)(estimatedHeight, maxSize.y));

  m_calculatedWindowSize = ImVec2(minSize.x, estimatedHeight);
  m_windowSizeCalculated = true;
}

// ===== NEW TEXT INPUT METHODS =====

void UserPromptUI::RequestInput(const std::string& title, const std::string& message,
  const std::string& defaultValue,
  std::function<void(const std::string&, bool)> callback) {

  std::lock_guard<std::mutex> lock(m_mutex);
  m_inputTitle = title;
  m_inputMessage = message;
  m_inputDefaultValue = defaultValue;
  m_inputBuffer = defaultValue;
  m_inputCallback = callback;
  m_showInputPrompt = true;
}

void UserPromptUI::RequestTextInput(const std::string& title,
  const std::string& message,
  const std::string& defaultValue,
  std::function<void(bool, const std::string&)> callback) {
  std::lock_guard<std::mutex> lock(m_mutex);

  printf("[DEBUG] RequestTextInput called: title='%s', default='%s'\n",
    title.c_str(), defaultValue.c_str());

  m_inputTitle = title;
  m_inputMessage = message;
  m_inputDefaultValue = defaultValue;
  m_inputBuffer = defaultValue;
  m_textInputCallback = callback;
  m_textInputRequested = true;
  m_showInputPrompt = true;
}

void UserPromptUI::CompleteTextInput(bool accepted, const std::string& value) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_textInputCallback) {
    m_textInputCallback(accepted, value);
  }
  if (m_inputCallback) {
    m_inputCallback(value, accepted);
  }

  m_textInputRequested = false;
  m_showInputPrompt = false;
  m_textInputCallback = nullptr;
  m_inputCallback = nullptr;
  m_inputBuffer.clear();
}

void UserPromptUI::RenderTextInputDialog() {
  if (!m_showInputPrompt) {
    return;
  }

  // Open popup on first frame
  static bool shouldOpen = false;
  if (m_showInputPrompt && !ImGui::IsPopupOpen("TextInputDialog")) {
    ImGui::OpenPopup("TextInputDialog");
    shouldOpen = true;
  }

  // Center the popup
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(450, 250), ImGuiCond_FirstUseEver);

  if (ImGui::BeginPopupModal("TextInputDialog", nullptr,
    ImGuiWindowFlags_AlwaysAutoResize)) {

    // Title with styling
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
    ImGui::Text("%s", m_inputTitle.c_str());
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // Message
    ImGui::TextWrapped("%s", m_inputMessage.c_str());
    ImGui::Spacing();

    // Show default value hint
    if (!m_inputDefaultValue.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
      ImGui::Text("Default: %s", m_inputDefaultValue.c_str());
      ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    // Text input field
    static char inputBuffer[256];
    if (shouldOpen) {
      strcpy_s(inputBuffer, sizeof(inputBuffer), m_inputBuffer.c_str());
      shouldOpen = false;
    }

    ImGui::Text("Enter value:");
    ImGui::SetNextItemWidth(-1);
    bool enterPressed = ImGui::InputText("##textinput", inputBuffer,
      sizeof(inputBuffer),
      ImGuiInputTextFlags_EnterReturnsTrue);

    // Focus on text input when window appears
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Buttons
    float buttonWidth = 120.0f;
    bool accepted = false;
    bool useDefault = false;
    bool cancelled = false;

    // Center buttons
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float totalButtonWidth = buttonWidth * 3 + 20;
    float startX = (availableWidth - totalButtonWidth) * 0.5f;
    if (startX > 0) {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);
    }

    // OK button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    if (ImGui::Button("OK", ImVec2(buttonWidth, 0)) || enterPressed) {
      accepted = true;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // Use Default button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
    if (ImGui::Button("Use Default", ImVec2(buttonWidth, 0))) {
      useDefault = true;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // Cancel button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
      cancelled = true;
    }
    ImGui::PopStyleColor();

    // Handle button actions
    if (accepted) {
      std::string finalValue(inputBuffer);
      if (finalValue.empty()) {
        finalValue = m_inputDefaultValue;
      }
      CompleteTextInput(true, finalValue);
      ImGui::CloseCurrentPopup();
    }
    else if (useDefault) {
      CompleteTextInput(true, m_inputDefaultValue);
      ImGui::CloseCurrentPopup();
    }
    else if (cancelled) {
      CompleteTextInput(false, m_inputDefaultValue);
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void UserPromptUI::Render() {
  // Check if we need to open a new prompt (thread-safe)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_promptRequested && !m_isPromptActive) {
      m_isPromptActive = true;
      m_promptRequested = false;
    }
  }

  // Render text input dialog if requested
  RenderTextInputDialog();

  if (!m_isVisible || !m_isPromptActive) {
    return;
  }

  CheckAutoConfirm();
  CalculateWindowSize();

  static bool needsFocus = false;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isPromptActive && !needsFocus) {
      needsFocus = true;
      ImGui::SetNextWindowFocus();
    }
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(m_calculatedWindowSize, ImGuiCond_Appearing);

  ImVec2 minSize = ImVec2(400, 250);
  ImVec2 maxSize = ImVec2(900, 700);
  ImGui::SetNextWindowSizeConstraints(minSize, maxSize);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;

  SetupPromptStyling();

  bool isOpen = true;
  std::string windowID = "User Confirmation Required##prompt_" + std::to_string((uintptr_t)this);

  if (ImGui::Begin(windowID.c_str(), &isOpen, flags)) {

    if (ImGui::IsWindowAppearing()) {
      ImGui::SetWindowFocus();
      needsFocus = false;
    }
    else if (m_isPromptActive && !ImGui::IsWindowFocused()) {
      ImGui::SetWindowFocus();
    }
    else if (ImGui::IsWindowFocused() && needsFocus) {
      needsFocus = false;
    }

    if (needsFocus) {
      ImGui::SetWindowFocus();
    }

    // Header with auto-confirm indicator
    if (m_autoConfirm) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
      ImGui::Text("USER CONFIRMATION REQUIRED (AUTO-CONFIRM)");
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.0f, 1.0f));
      ImGui::Text("USER CONFIRMATION REQUIRED");
    }
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // Title (if different from default)
    if (!m_title.empty() && m_title != "User Confirmation") {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
      ImGui::Text("Title: %s", m_title.c_str());
      ImGui::PopStyleColor();
      ImGui::Spacing();
    }

    // Message with fixed scrollable area
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

    float reservedHeight = 200;
    float availableHeight = ImGui::GetContentRegionAvail().y - reservedHeight;

    if (availableHeight < 60) availableHeight = 60;

    ImGui::Text("Message:");

    ImGui::BeginChild("MessageScroll", ImVec2(0, availableHeight), true,
      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextWrapped("%s", m_message.c_str());
    ImGui::EndChild();

    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Auto-confirm countdown display (if enabled)
    if (m_autoConfirm && !m_autoConfirmTriggered) {
      float elapsed = GetCurrentTime() - m_promptStartTime;
      float remaining = m_autoConfirmDelay - elapsed;

      if (remaining > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
        ImGui::Text("Auto-confirming in %.1f seconds...", remaining);
        ImGui::PopStyleColor();

        float progress = elapsed / m_autoConfirmDelay;
        ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
      }
      else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Text("Auto-confirming NOW...");
        ImGui::PopStyleColor();
      }
      ImGui::Spacing();
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.3f, 0.0f, 1.0f));
      ImGui::Text("Program execution is PAUSED - Waiting for your decision...");
      ImGui::PopStyleColor();
      ImGui::Spacing();
    }

    // Fixed button layout
    float buttonWidth = 120.0f;
    float buttonHeight = 40.0f;
    float buttonSpacing = 15.0f;

    float availableWidth = ImGui::GetContentRegionAvail().x;
    float totalButtonWidth = buttonWidth * 3 + buttonSpacing * 2;

    float startX = (availableWidth - totalButtonWidth) * 0.5f;
    if (startX > 0) {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);
    }

    // YES button
    std::string yesID = "YES##prompt_yes_" + std::to_string((uintptr_t)this);
    if (m_autoConfirm && !m_autoConfirmTriggered) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    }
    else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    if (ImGui::Button(yesID.c_str(), ImVec2(buttonWidth, buttonHeight))) {
      OnYesClicked();
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0, buttonSpacing);

    // NO button
    std::string noID = "NO##prompt_no_" + std::to_string((uintptr_t)this);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    if (ImGui::Button(noID.c_str(), ImVec2(buttonWidth, buttonHeight))) {
      OnNoClicked();
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0, buttonSpacing);

    // CANCEL button
    std::string cancelID = "CANCEL##prompt_cancel_" + std::to_string((uintptr_t)this);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    if (ImGui::Button(cancelID.c_str(), ImVec2(buttonWidth, buttonHeight))) {
      OnCancelClicked();
    }
    ImGui::PopStyleColor(4);

    ImGui::Spacing();

    // Help text with auto-confirm info
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    if (m_autoConfirm) {
      ImGui::TextWrapped("YES = Continue | NO/CANCEL = Stop | Auto-confirm: %.1fs", m_autoConfirmDelay);
    }
    else {
      ImGui::TextWrapped("YES = Continue | NO/CANCEL = Stop program");
    }
    ImGui::PopStyleColor();

    ImGui::End();
  }

  RestoreDefaultStyling();

  if (!isOpen) {
    OnCancelClicked();
  }
}

void UserPromptUI::CheckAutoConfirm() {
  if (!m_autoConfirm || m_autoConfirmTriggered || !m_isPromptActive) {
    return;
  }

  float elapsed = GetCurrentTime() - m_promptStartTime;

  if (elapsed >= m_autoConfirmDelay) {
    printf("[DEBUG] Auto-confirm triggered after %.1f seconds\n", elapsed);
    m_autoConfirmTriggered = true;
    OnYesClicked();
  }
}

PromptResult UserPromptUI::GetResult() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_result.load();
}

void UserPromptUI::Reset() {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_isPromptActive = false;
  m_result = PromptResult::PENDING;
  m_callback = nullptr;
  m_autoConfirmTriggered = false;
  m_windowSizeCalculated = false;
}

void UserPromptUI::OnYesClicked() {
  printf("[DEBUG] OnYesClicked() called%s\n", m_autoConfirmTriggered ? " (auto-confirmed)" : "");

  std::lock_guard<std::mutex> lock(m_mutex);

  m_result = PromptResult::YES;
  m_isPromptActive = false;
  m_isVisible = false;

  if (m_callback) {
    m_callback(PromptResult::YES);
  }
}

void UserPromptUI::OnNoClicked() {
  printf("[DEBUG] OnNoClicked() called\n");

  std::lock_guard<std::mutex> lock(m_mutex);

  m_result = PromptResult::NO;
  m_isPromptActive = false;
  m_isVisible = false;
  m_autoConfirmTriggered = true;

  if (m_callback) {
    m_callback(PromptResult::NO);
  }
}

void UserPromptUI::OnCancelClicked() {
  printf("[DEBUG] OnCancelClicked() called\n");

  std::lock_guard<std::mutex> lock(m_mutex);

  m_result = PromptResult::CANCELLED;
  m_isPromptActive = false;
  m_isVisible = false;
  m_autoConfirmTriggered = true;

  if (m_callback) {
    m_callback(PromptResult::CANCELLED);
  }
}

void UserPromptUI::SetupPromptStyling() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
}

void UserPromptUI::RestoreDefaultStyling() {
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(3);
}

void UserPromptUI::SetupPromptStylingDark() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
}