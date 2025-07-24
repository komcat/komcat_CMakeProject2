// UserPromptUI.cpp
#include "UserPromptUI.h"
#include <algorithm>
#include <chrono>

UserPromptUI::UserPromptUI()
  : m_isVisible(false)
  , m_isPromptActive(false)
  , m_promptRequested(false)
  , m_title("User Confirmation")
  , m_message("")
  , m_result(PromptResult::PENDING)
  , m_autoConfirm(false)           // NEW: Default auto-confirm off
  , m_autoConfirmDelay(3.0f)       // NEW: Default 3 seconds
  , m_promptStartTime(0.0f)        // NEW: Track when prompt started
  , m_autoConfirmTriggered(false)  // NEW: Prevent multiple triggers
{
}

UserPromptUI::~UserPromptUI() {
}

// NEW: Get current time helper
float UserPromptUI::GetCurrentTime() const {
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration<float>(duration).count();
}

// Thread-safe method that just sets flags
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

  // NEW: Reset auto-confirm state for new prompt
  m_autoConfirmTriggered = false;
  m_promptStartTime = GetCurrentTime();

  printf("[DEBUG] RequestPrompt: Auto-confirm will trigger in %.1f seconds\n", m_autoConfirmDelay);
}

// Keep the old ShowPrompt method for backward compatibility
void UserPromptUI::ShowPrompt(const std::string& title, const std::string& message,
  std::function<void(PromptResult)> callback) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_title = title;
  m_message = message;
  m_callback = callback;
  m_result = PromptResult::PENDING;
  m_isPromptActive = true;
  m_isVisible = true;

  // NEW: Reset auto-confirm state
  m_autoConfirmTriggered = false;
  m_promptStartTime = GetCurrentTime();

  ImGui::OpenPopup(m_title.c_str());
}

// Update UserPromptUI.cpp Render() method with better layout handling

void UserPromptUI::Render() {
  // Check if we need to open a new prompt (thread-safe)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_promptRequested && !m_isPromptActive) {
      m_isPromptActive = true;
      m_promptRequested = false;
    }
  }

  if (!m_isVisible || !m_isPromptActive) {
    return;
  }

  // Check auto-confirm before rendering
  CheckAutoConfirm();

  // Force focus on the next window if prompt just became active
  static bool needsFocus = false;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isPromptActive && !needsFocus) {
      needsFocus = true;
      ImGui::SetNextWindowFocus();
    }
  }

  // IMPROVED: Dynamic window sizing based on content
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  // Calculate content-based window size
  ImVec2 minSize = ImVec2(500, 300);
  ImVec2 maxSize = ImVec2(800, 600);

  // Estimate content height
  float estimatedHeight = 120;  // Base height for header and buttons

  // Add height for title
  if (!m_title.empty() && m_title != "User Confirmation") {
    estimatedHeight += 30;
  }

  // Add height for message (rough estimate based on text length)
  float messageLines = (float)m_message.length() / 80.0f;  // Estimate characters per line
  if (messageLines < 1.0f) messageLines = 1.0f;
  estimatedHeight += messageLines * 20.0f + 40;  // Line height + spacing

  // Add height for auto-confirm display
  if (m_autoConfirm && !m_autoConfirmTriggered) {
    estimatedHeight += 60;  // Progress bar and countdown text
  }

  // Clamp to min/max
  estimatedHeight = std::max(minSize.y, std::min(estimatedHeight, maxSize.y));

  ImVec2 windowSize = ImVec2(minSize.x, estimatedHeight);
  ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(minSize, maxSize);

  // IMPROVED: Better window flags for resizing
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoDocking |
    ImGuiWindowFlags_AlwaysAutoResize;  // NEW: Auto-resize to content

  SetupPromptStyling();

  bool isOpen = true;
  std::string windowID = "User Confirmation Required##prompt_" + std::to_string((uintptr_t)this);

  if (ImGui::Begin(windowID.c_str(), &isOpen, flags)) {

    // Make window focused
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

    // IMPROVED: Message with scrollable area for long content
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

    // Calculate available space for message
    float availableHeight = ImGui::GetContentRegionAvail().y - 150; // Reserve space for buttons and status
    if (availableHeight < 50) availableHeight = 50; // Minimum message area

    // Create scrollable region for message if content is long
    if (m_message.length() > 200 || availableHeight < 100) {  // Use scrolling for long messages
      ImGui::Text("Message:");
      ImGui::BeginChild("MessageScroll", ImVec2(0, availableHeight), true,
        ImGuiWindowFlags_HorizontalScrollbar);
      ImGui::TextWrapped("%s", m_message.c_str());
      ImGui::EndChild();
    }
    else {
      // Short message - no scrolling needed
      ImGui::Text("Message:");
      ImGui::TextWrapped("%s", m_message.c_str());
    }

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

        // Progress bar showing countdown
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
      // Normal status message
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.3f, 0.0f, 1.0f));
      ImGui::Text("Program execution is PAUSED - Waiting for your decision...");
      ImGui::PopStyleColor();
      ImGui::Spacing();
    }

    // IMPROVED: Responsive button layout
    float buttonWidth = 120.0f;
    float buttonHeight = 40.0f;
    float buttonSpacing = 15.0f;

    // Calculate button layout
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float totalButtonWidth = buttonWidth * 3 + buttonSpacing * 2;

    // Adjust button size if window is too narrow
    if (totalButtonWidth > availableWidth) {
      buttonWidth = (availableWidth - buttonSpacing * 2) / 3.0f;
      if (buttonWidth < 80) {
        // Stack buttons vertically if too narrow
        buttonWidth = availableWidth * 0.8f;
        buttonSpacing = 5.0f;
      }
    }

    // Center buttons horizontally
    float startX = (availableWidth - (buttonWidth * 3 + buttonSpacing * 2)) * 0.5f;
    if (startX > 0 && totalButtonWidth <= availableWidth) {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);
    }

    // YES button - highlight if auto-confirm is active
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

    // Check if we should stack buttons vertically
    bool stackVertically = (buttonWidth >= availableWidth * 0.7f);

    if (!stackVertically) {
      ImGui::SameLine(0, buttonSpacing);
    }

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

    if (!stackVertically) {
      ImGui::SameLine(0, buttonSpacing);
    }

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

  // Handle window close
  if (!isOpen) {
    OnCancelClicked();
  }
}


// NEW: Check and handle auto-confirm
void UserPromptUI::CheckAutoConfirm() {
  if (!m_autoConfirm || m_autoConfirmTriggered || !m_isPromptActive) {
    return;
  }

  float elapsed = GetCurrentTime() - m_promptStartTime;

  if (elapsed >= m_autoConfirmDelay) {
    printf("[DEBUG] Auto-confirm triggered after %.1f seconds\n", elapsed);
    m_autoConfirmTriggered = true;
    OnYesClicked();  // Auto-confirm always selects YES
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
  m_autoConfirmTriggered = false;  // NEW: Reset auto-confirm state
}

// Button handlers
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
  m_autoConfirmTriggered = true;  // NEW: Stop auto-confirm

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
  m_autoConfirmTriggered = true;  // NEW: Stop auto-confirm

  if (m_callback) {
    m_callback(PromptResult::CANCELLED);
  }
}

// UI styling methods (unchanged)
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