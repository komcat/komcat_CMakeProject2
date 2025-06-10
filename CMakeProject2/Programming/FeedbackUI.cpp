#include "FeedbackUI.h"
#include <algorithm>

FeedbackUI::FeedbackUI()
  : m_isVisible(false)
  , m_title("Block Execution Results") {
}

FeedbackUI::~FeedbackUI() {
}

void FeedbackUI::Render() {
  if (!m_isVisible) return;

  // Set window size and position
  ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

  // Create the window
  if (ImGui::Begin(m_title.c_str(), &m_isVisible)) {

    // Control buttons
    if (ImGui::Button("Clear All")) {
      ClearBlocks();
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) {
      Hide();
    }

    ImGui::Separator();

    // Table with results
    if (ImGui::BeginTable("BlockResults", 5,
      ImGuiTableFlags_Borders |
      ImGuiTableFlags_RowBg |
      ImGuiTableFlags_ScrollY |
      ImGuiTableFlags_Resizable)) {

      RenderTableHeader();

      // Render each block
      for (const auto& block : m_blocks) {
        RenderTableRow(block);
      }

      ImGui::EndTable();
    }
  }
  ImGui::End();
}

void FeedbackUI::SetBlocks(const std::vector<BlockResult>& blocks) {
  m_blocks = blocks;
}

void FeedbackUI::AddBlock(const BlockResult& block) {
  m_blocks.push_back(block);
}

void FeedbackUI::UpdateBlock(const std::string& gridId, const std::string& response1,
  const std::string& response2, const std::string& response3) {
  auto it = std::find_if(m_blocks.begin(), m_blocks.end(),
    [&gridId](const BlockResult& block) { return block.gridId == gridId; });

  if (it != m_blocks.end()) {
    it->response1 = response1;
    it->response2 = response2;
    it->response3 = response3;
  }
}

void FeedbackUI::ClearBlocks() {
  m_blocks.clear();
}

ImVec4 FeedbackUI::GetStatusColor(const std::string& status) {
  std::string statusLower = status;
  std::transform(statusLower.begin(), statusLower.end(), statusLower.begin(), ::tolower);

  if (statusLower.find("complete") != std::string::npos ||
    statusLower.find("success") != std::string::npos) {
    return ImVec4(0.0f, 0.8f, 0.0f, 1.0f); // Green
  }
  else if (statusLower.find("incomplete") != std::string::npos ||
    statusLower.find("error") != std::string::npos ||
    statusLower.find("failed") != std::string::npos) {
    return ImVec4(0.8f, 0.0f, 0.0f, 1.0f); // Red
  }
  else if (statusLower.find("pending") != std::string::npos ||
    statusLower.find("waiting") != std::string::npos) {
    return ImVec4(0.8f, 0.8f, 0.0f, 1.0f); // Yellow
  }
  else {
    return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
  }
}

void FeedbackUI::RenderTableHeader() {
  ImGui::TableSetupColumn("Grid ID", ImGuiTableColumnFlags_WidthFixed, 80);
  ImGui::TableSetupColumn("Block Name", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100);
  ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 100);
  ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableHeadersRow();
}

void FeedbackUI::RenderTableRow(const BlockResult& block) {
  ImGui::TableNextRow();

  // Grid ID
  ImGui::TableNextColumn();
  ImGui::Text("%s", block.gridId.c_str());

  // Block Name
  ImGui::TableNextColumn();
  ImGui::Text("%s", block.blockName.c_str());

  // Status with color
  ImGui::TableNextColumn();
  ImVec4 statusColor = GetStatusColor(block.response1);
  ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
  ImGui::Text("%s", block.response1.c_str());
  ImGui::PopStyleColor();

  // Result with color
  ImGui::TableNextColumn();
  ImVec4 resultColor = GetStatusColor(block.response2);
  ImGui::PushStyleColor(ImGuiCol_Text, resultColor);
  ImGui::Text("%s", block.response2.c_str());
  ImGui::PopStyleColor();

  // Details
  ImGui::TableNextColumn();
  ImGui::Text("%s", block.response3.c_str());
}