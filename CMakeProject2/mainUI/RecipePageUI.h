#pragma once
#include "imgui.h"
#include "ProcessRegistry.h"  // ADD THIS
#include <string>
#include <vector>

class RecipePageUI {
public:
  RecipePageUI();
  ~RecipePageUI();

  void RenderUI();

  // Getters for state (if needed by MainUIManager)
  bool IsRecipeModified() const { return m_isModified; }
  const std::string& GetCurrentRecipeName() const { return m_currentRecipeName; }

private:
  // UI State
  bool m_newRecipeClicked = false;
  bool m_isModified = false;
  char m_recipeNameBuffer[256] = "";
  std::string m_currentRecipeName = "";

  // ADD THESE - Process selection state
  bool m_showProcessSelection = false;
  std::string m_selectedCategory = "All";
  std::vector<std::string> m_addedProcesses;  // Track added processes

  // Layout constants
  static constexpr float LEFT_COLUMN_WIDTH_RATIO = 0.25f;
  static constexpr float CENTER_COLUMN_WIDTH_RATIO = 0.25f;
  static constexpr float RIGHT_COLUMN_WIDTH_RATIO = 0.50f;

  // Column render methods
  void RenderLeftColumn();
  void RenderCenterColumn();
  void RenderRightColumn();

  // Button action methods (placeholders for now)
  void OnNewRecipeClicked();
  void OnLoadRecipeClicked();
  void OnSaveRecipeClicked();
  void OnAddProcessInstanceClicked();

  // ADD THESE - New methods for process selection
  void OnProcessSelected(const std::string& processName);
  void RenderProcessSelectionUI();
  void RenderProcessList(const std::vector<std::string>& processes);

  // Helper methods
  void ClearRecipeData();
  bool ValidateRecipeName() const;
};