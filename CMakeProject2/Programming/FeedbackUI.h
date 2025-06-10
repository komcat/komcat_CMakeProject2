#pragma once

#include <imgui.h>
#include <string>
#include <vector>

struct BlockResult {
  std::string gridId;
  std::string blockName;
  std::string response1;  // Status (Complete/Incomplete/Pending)
  std::string response2;  // Result (Success/Error/Waiting)
  std::string response3;  // Details (Time/Error message/etc)

  BlockResult(const std::string& id, const std::string& name,
    const std::string& r1, const std::string& r2, const std::string& r3)
    : gridId(id), blockName(name), response1(r1), response2(r2), response3(r3) {
  }
};

class FeedbackUI {
public:
  FeedbackUI();
  ~FeedbackUI();

  // Main render function
  void Render();

  // Control visibility
  void Show() { m_isVisible = true; }
  void Hide() { m_isVisible = false; }
  bool IsVisible() const { return m_isVisible; }

  // Data management
  void SetBlocks(const std::vector<BlockResult>& blocks);
  void AddBlock(const BlockResult& block);
  void UpdateBlock(const std::string& gridId, const std::string& response1,
    const std::string& response2, const std::string& response3);
  void ClearBlocks();

  // Configuration
  void SetTitle(const std::string& title) { m_title = title; }

private:
  bool m_isVisible;
  std::string m_title;
  std::vector<BlockResult> m_blocks;

  // UI helpers
  ImVec4 GetStatusColor(const std::string& status);
  void RenderTableHeader();
  void RenderTableRow(const BlockResult& block);
};