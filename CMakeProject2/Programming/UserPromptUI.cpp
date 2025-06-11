// ═══════════════════════════════════════════════════════════════════════════════════════
// STEP 2: Create UserPromptUI.cpp - Implementation
// ═══════════════════════════════════════════════════════════════════════════════════════

// UserPromptUI.cpp
#include "UserPromptUI.h"
#include <algorithm>


UserPromptUI::UserPromptUI()
  : m_isVisible(false)
  , m_isPromptActive(false)
  , m_promptRequested(false)  // NEW
  , m_title("User Confirmation")
  , m_message("")
  , m_result(PromptResult::PENDING) {
}

// NEW: Thread-safe method that just sets flags
// ===================================================================
// DEBUG: Add logging to see if RequestPrompt is called
// ===================================================================

void UserPromptUI::RequestPrompt(const std::string& title, const std::string& message,
  std::function<void(PromptResult)> callback) {

  printf("[DEBUG] RequestPrompt called: title='%s', message='%s'\n", title.c_str(), message.c_str());

  std::lock_guard<std::mutex> lock(m_mutex);

  m_title = title;
  m_message = message;
  m_callback = callback;
  m_result = PromptResult::PENDING;
  m_promptRequested = true;
  m_isVisible = true;

  printf("[DEBUG] RequestPrompt: m_promptRequested set to true, m_isVisible set to true\n");
}

// Keep the old ShowPrompt method for backward compatibility
void UserPromptUI::ShowPrompt(const std::string& title, const std::string& message,
  std::function<void(PromptResult)> callback) {
  // This should only be called from the main thread
  std::lock_guard<std::mutex> lock(m_mutex);

  m_title = title;
  m_message = message;
  m_callback = callback;
  m_result = PromptResult::PENDING;
  m_isPromptActive = true;
  m_isVisible = true;

  ImGui::OpenPopup(m_title.c_str());  // Direct call - main thread only
}

UserPromptUI::~UserPromptUI() {
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

  if (!m_isVisible || !m_isPromptActive) return;

  // UPDATED: Center window only when it first appears, then let user move it
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); // Changed to Appearing
  ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_Appearing);          // Changed to Appearing

  // UPDATED: Remove NoMove flag to make window movable
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse;

  SetupPromptStyling();

  bool isOpen = true;

  // CRITICAL: Use unique window ID with object pointer
  std::string windowID = "User Confirmation Required##prompt_" + std::to_string((uintptr_t)this);

  if (ImGui::Begin(windowID.c_str(), &isOpen, flags)) {

    // Header
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.0f, 1.0f));
    ImGui::Text("USER CONFIRMATION REQUIRED");
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // Title
    if (!m_title.empty() && m_title != "User Confirmation") {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
      ImGui::Text("Title: %s", m_title.c_str());
      ImGui::PopStyleColor();
      ImGui::Spacing();
    }

    // Message
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::TextWrapped("Message: %s", m_message.c_str());
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Status
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.3f, 0.0f, 1.0f));
    ImGui::Text("Program execution is PAUSED - Waiting for your decision...");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Spacing();

    // FIXED: Use ## syntax for absolutely unique button IDs
    float buttonWidth = 150.0f;
    float buttonHeight = 50.0f;

    // Center buttons
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float totalWidth = buttonWidth * 3 + 20.0f * 2;
    float startX = (availableWidth - totalWidth) * 0.5f;
    if (startX > 0) {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);
    }

    // YES button - GUARANTEED unique ID using object pointer
    std::string yesID = "YES##prompt_yes_" + std::to_string((uintptr_t)this);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    if (ImGui::Button(yesID.c_str(), ImVec2(buttonWidth, buttonHeight))) {
      printf("[DEBUG] YES button clicked!\n");
      OnYesClicked();
    }

    ImGui::PopStyleColor(4);
    ImGui::SameLine(0, 20.0f);

    // NO button - GUARANTEED unique ID
    std::string noID = "NO##prompt_no_" + std::to_string((uintptr_t)this);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    if (ImGui::Button(noID.c_str(), ImVec2(buttonWidth, buttonHeight))) {
      printf("[DEBUG] NO button clicked!\n");
      OnNoClicked();
    }

    ImGui::PopStyleColor(4);
    ImGui::SameLine(0, 20.0f);

    // CANCEL button - GUARANTEED unique ID
    std::string cancelID = "CANCEL##prompt_cancel_" + std::to_string((uintptr_t)this);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    if (ImGui::Button(cancelID.c_str(), ImVec2(buttonWidth, buttonHeight))) {
      printf("[DEBUG] CANCEL button clicked!\n");
      OnCancelClicked();
    }

    ImGui::PopStyleColor(4);

    ImGui::Spacing();
    ImGui::Spacing();

    // Help text
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::TextWrapped("YES = Continue program execution | NO/CANCEL = Stop program");
    ImGui::PopStyleColor();

    ImGui::End();
  }

  RestoreDefaultStyling();

  // Handle window close
  if (!isOpen) {
    OnCancelClicked();
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
}



// ===================================================================
// ADD DEBUG to button handlers
// ===================================================================

void UserPromptUI::OnYesClicked() {
  printf("[DEBUG] OnYesClicked() called\n");

  std::lock_guard<std::mutex> lock(m_mutex);

  m_result = PromptResult::YES;
  m_isPromptActive = false;
  m_isVisible = false;

  printf("[DEBUG] Set result to YES, closing prompt\n");

  if (m_callback) {
    printf("[DEBUG] Calling callback with YES\n");
    m_callback(PromptResult::YES);
  }
  else {
    printf("[DEBUG] ERROR: No callback set!\n");
  }
}

void UserPromptUI::OnNoClicked() {
  printf("[DEBUG] OnNoClicked() called\n");

  std::lock_guard<std::mutex> lock(m_mutex);

  m_result = PromptResult::NO;
  m_isPromptActive = false;
  m_isVisible = false;

  printf("[DEBUG] Set result to NO, closing prompt\n");

  if (m_callback) {
    printf("[DEBUG] Calling callback with NO\n");
    m_callback(PromptResult::NO);
  }
  else {
    printf("[DEBUG] ERROR: No callback set!\n");
  }
}

void UserPromptUI::OnCancelClicked() {
  printf("[DEBUG] OnCancelClicked() called\n");

  std::lock_guard<std::mutex> lock(m_mutex);

  m_result = PromptResult::CANCELLED;
  m_isPromptActive = false;
  m_isVisible = false;

  printf("[DEBUG] Set result to CANCELLED, closing prompt\n");

  if (m_callback) {
    printf("[DEBUG] Calling callback with CANCELLED\n");
    m_callback(PromptResult::CANCELLED);
  }
  else {
    printf("[DEBUG] ERROR: No callback set!\n");
  }
}



// ===================================================================
// ENHANCED SetupPromptStyling() for better window appearance
// ===================================================================

void UserPromptUI::SetupPromptStyling() {
  // Make the window more prominent with better styling
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

  // IMPROVED: Better window background color
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.95f, 0.95f, 0.95f, 1.0f)); // Light gray background
  ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
}

void UserPromptUI::RestoreDefaultStyling() {
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(3);
}

// ===================================================================
// ALTERNATIVE: Dark Theme Version (if you prefer dark background)
// ===================================================================

void UserPromptUI::SetupPromptStylingDark() {
  // Dark theme version for better contrast
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

  // Dark background
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f)); // Dark background
  ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
}