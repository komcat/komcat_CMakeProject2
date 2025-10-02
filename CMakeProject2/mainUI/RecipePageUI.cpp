#include "RecipePageUI.h"
#include <iostream>
#include <cstring>
#include <algorithm>

RecipePageUI::RecipePageUI() {
  std::cout << "RecipePageUI: Initialized" << std::endl;
}

RecipePageUI::~RecipePageUI() = default;

void RecipePageUI::RenderUI() {
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("Recipe Management");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Get available content area
  ImVec2 contentSize = ImGui::GetContentRegionAvail();

  // Calculate column widths
  float leftWidth = contentSize.x * LEFT_COLUMN_WIDTH_RATIO;
  float centerWidth = contentSize.x * CENTER_COLUMN_WIDTH_RATIO;
  float rightWidth = contentSize.x * RIGHT_COLUMN_WIDTH_RATIO;

  // Render 3-column layout with embedded style
  if (ImGui::BeginTable("RecipeLayout", 3,
    ImGuiTableFlags_Borders |
    ImGuiTableFlags_Resizable |
    ImGuiTableFlags_BordersInnerV)) {

    // Setup columns with specified widths
    ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, leftWidth);
    ImGui::TableSetupColumn("Recipe", ImGuiTableColumnFlags_WidthFixed, centerWidth);
    ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, rightWidth);

    // Optional: Add headers
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();

    // Left Column (25%)
    ImGui::TableNextColumn();
    RenderLeftColumn();

    // Center Column (25%)
    ImGui::TableNextColumn();
    RenderCenterColumn();

    // Right Column (50%)
    ImGui::TableNextColumn();
    RenderRightColumn();

    ImGui::EndTable();
  }
}

void RecipePageUI::RenderLeftColumn() {
  ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "Recipe Control");
  ImGui::Separator();
  ImGui::Spacing();

  // New Recipe Button
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));

  if (ImGui::Button("New Recipe", ImVec2(-1, 40))) {
    OnNewRecipeClicked();
  }

  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar();

  ImGui::Spacing();

  // Load/Save buttons section
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "File Operations");
  ImGui::Spacing();

  // Load Recipe Button
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));

  if (ImGui::Button("Load Recipe", ImVec2(-1, 35))) {
    OnLoadRecipeClicked();
  }

  ImGui::PopStyleColor(2);

  ImGui::Spacing();

  // Save Recipe Button
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.5f, 0.3f, 1.0f));

  // Disable save button if no recipe is loaded
  bool canSave = !m_currentRecipeName.empty() || m_newRecipeClicked;
  if (!canSave) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  }

  if (ImGui::Button("Save Recipe", ImVec2(-1, 35)) && canSave) {
    OnSaveRecipeClicked();
  }

  if (!canSave) {
    ImGui::PopStyleVar();
  }

  ImGui::PopStyleColor(2);

  // Show save status
  if (!canSave) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(No recipe loaded)");
  }
  else if (m_isModified) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Modified)");
  }
}

void RecipePageUI::RenderCenterColumn() {
  if (m_newRecipeClicked) {
    ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.8f, 1.0f), "New Recipe");
    ImGui::Separator();
    ImGui::Spacing();

    // Recipe Name Input
    ImGui::Text("Name:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##RecipeName", m_recipeNameBuffer, sizeof(m_recipeNameBuffer))) {
      m_isModified = true;
    }

    ImGui::Spacing();

    // Add Process Instances Button
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.2f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.3f, 0.7f, 1.0f));

    if (ImGui::Button("Add Process Instance", ImVec2(-1, 35))) {
      OnAddProcessInstanceClicked();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    ImGui::Spacing();

    // Show added processes
    if (!m_addedProcesses.empty()) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Added Processes:");
      for (size_t i = 0; i < m_addedProcesses.size(); i++) {
        ImGui::Text("%zu. %s", i + 1, m_addedProcesses[i].c_str());

        // Add remove button
        ImGui::SameLine();
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton("Remove")) {
          m_addedProcesses.erase(m_addedProcesses.begin() + i);
          m_isModified = true;
          break;  // Break to avoid iterator invalidation
        }
        ImGui::PopID();
      }
    }

    ImGui::Spacing();

    // Validation feedback
    if (strlen(m_recipeNameBuffer) == 0) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Enter recipe name");
    }
    else if (ValidateRecipeName()) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Name is valid");
    }
    else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid name format");
    }

  }
  else {
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Recipe Editor");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Click 'New Recipe' to create");
    ImGui::Text("or 'Load Recipe' to edit");
    ImGui::Text("an existing recipe.");

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No recipe loaded");
  }
}

void RecipePageUI::RenderRightColumn() {
  if (m_showProcessSelection) {
    RenderProcessSelectionUI();
  }
  else {
    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.8f, 1.0f), "Recipe Preview");
    ImGui::Separator();
    ImGui::Spacing();

    if (m_newRecipeClicked && strlen(m_recipeNameBuffer) > 0) {
      ImGui::Text("Recipe: %s", m_recipeNameBuffer);
      ImGui::Spacing();

      ImGui::Text("Process Instances (%zu):", m_addedProcesses.size());
      if (m_addedProcesses.empty()) {
        ImGui::BulletText("(None yet - click 'Add Process Instance')");
      }
      else {
        for (size_t i = 0; i < m_addedProcesses.size(); i++) {
          ImGui::BulletText("%zu. %s", i + 1, m_addedProcesses[i].c_str());
        }
      }

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.0f, 1.0f), "Preview Area");
      ImGui::Text("This area will show:");
      ImGui::BulletText("Recipe structure");
      ImGui::BulletText("Process flow diagram");
      ImGui::BulletText("Parameter validation");
      ImGui::BulletText("Execution preview");

    }
    else {
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No recipe to preview");
      ImGui::Spacing();

      ImGui::Text("Recipe preview will show here");
      ImGui::Text("when a recipe is loaded or created.");
    }
  }
}

// NEW: Process selection UI
void RecipePageUI::RenderProcessSelectionUI() {
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Select Process to Add");
  ImGui::Separator();
  ImGui::Spacing();

  // Back button
  if (ImGui::Button("<< Back to Preview", ImVec2(-1, 30))) {
    m_showProcessSelection = false;
  }

  ImGui::Spacing();

  // Get available categories
  auto& registry = ProcessRegistry::GetInstance();
  auto categories = registry.GetAllCategories();

  // Add "All" option
  std::vector<std::string> categoryOptions = { "All" };
  categoryOptions.insert(categoryOptions.end(), categories.begin(), categories.end());

  // Category filter
  ImGui::Text("Category Filter:");
  ImGui::SetNextItemWidth(-1);
  if (ImGui::BeginCombo("##CategoryFilter", m_selectedCategory.c_str())) {
    for (const auto& category : categoryOptions) {
      bool isSelected = (m_selectedCategory == category);
      if (ImGui::Selectable(category.c_str(), isSelected)) {
        m_selectedCategory = category;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::Spacing();

  // Get processes to display
  std::vector<std::string> processesToShow;
  if (m_selectedCategory == "All") {
    processesToShow = registry.GetAllProcessNames();
  }
  else {
    processesToShow = registry.GetProcessesByCategory(m_selectedCategory);
  }

  // Show process count
  ImGui::Text("Available Processes: %zu", processesToShow.size());
  ImGui::Separator();

  // Render process list
  RenderProcessList(processesToShow);
}

void RecipePageUI::RenderProcessList(const std::vector<std::string>& processes) {
  auto& registry = ProcessRegistry::GetInstance();

  // Scrollable area for process list
  if (ImGui::BeginChild("ProcessList", ImVec2(0, -1), true)) {
    for (const auto& processName : processes) {
      const ProcessInfo* info = registry.GetProcessInfo(processName);

      // Process name button
      ImGui::PushID(processName.c_str());

      // Check if already added
      bool alreadyAdded = std::find(m_addedProcesses.begin(), m_addedProcesses.end(), processName) != m_addedProcesses.end();

      if (alreadyAdded) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::Button(processName.c_str(), ImVec2(-1, 30));
        ImGui::PopStyleColor(2);
      }
      else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));

        if (ImGui::Button(processName.c_str(), ImVec2(-1, 30))) {
          OnProcessSelected(processName);
        }

        ImGui::PopStyleColor(2);
      }

      // Show tooltip with process info
      if (ImGui::IsItemHovered() && info) {
        ImGui::BeginTooltip();
        ImGui::Text("Category: %s", info->category.c_str());
        ImGui::Text("Description: %s", info->description.c_str());
        if (alreadyAdded) {
          ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Already added to recipe");
        }
        ImGui::EndTooltip();
      }

      ImGui::PopID();
      ImGui::Spacing();
    }
  }
  ImGui::EndChild();
}

// Button action methods
void RecipePageUI::OnNewRecipeClicked() {
  m_newRecipeClicked = true;
  m_isModified = false;
  m_showProcessSelection = false;  // Reset process selection view
  ClearRecipeData();
  std::cout << "RecipePageUI: New Recipe clicked" << std::endl;
}

void RecipePageUI::OnLoadRecipeClicked() {
  std::cout << "RecipePageUI: Load Recipe clicked (placeholder)" << std::endl;
  // TODO: Implement file dialog and recipe loading
}

void RecipePageUI::OnSaveRecipeClicked() {
  if (ValidateRecipeName()) {
    m_currentRecipeName = std::string(m_recipeNameBuffer);
    m_isModified = false;
    std::cout << "RecipePageUI: Save Recipe clicked - " << m_currentRecipeName << std::endl;
    std::cout << "RecipePageUI: Process count: " << m_addedProcesses.size() << std::endl;
    // TODO: Implement actual save functionality
  }
}

void RecipePageUI::OnAddProcessInstanceClicked() {
  m_showProcessSelection = true;
  std::cout << "RecipePageUI: Add Process Instance clicked - showing process selection" << std::endl;

  // Debug: Print available processes
  auto& registry = ProcessRegistry::GetInstance();
  std::cout << "Available processes: " << registry.GetProcessCount() << std::endl;
  registry.PrintAllProcesses();
}

void RecipePageUI::OnProcessSelected(const std::string& processName) {
  // Add to recipe
  m_addedProcesses.push_back(processName);
  m_isModified = true;
  m_showProcessSelection = false;  // Return to preview

  std::cout << "RecipePageUI: Added process: " << processName << std::endl;
  std::cout << "RecipePageUI: Total processes in recipe: " << m_addedProcesses.size() << std::endl;
}

// Helper methods
void RecipePageUI::ClearRecipeData() {
  memset(m_recipeNameBuffer, 0, sizeof(m_recipeNameBuffer));
  m_currentRecipeName.clear();
  m_addedProcesses.clear();  // Clear added processes
  m_isModified = false;
}

bool RecipePageUI::ValidateRecipeName() const {
  if (strlen(m_recipeNameBuffer) == 0) return false;
  if (strlen(m_recipeNameBuffer) > 100) return false;

  // Simple validation - no special characters except underscore and dash
  for (size_t i = 0; i < strlen(m_recipeNameBuffer); i++) {
    char c = m_recipeNameBuffer[i];
    if (!((c >= 'a' && c <= 'z') ||
      (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') ||
      c == '_' || c == '-' || c == ' ')) {
      return false;
    }
  }

  return true;
}