#include "RecipePageUI.h"
#include "ProcessParameterFactory.h"
#include "CoreProcessParameterRegistration.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>


RecipePageUI::RecipePageUI() {
  std::cout << "RecipePageUI: Initialized" << std::endl;

  RegisterCoreProcessParameters();  // ADD THIS LINE
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

    // Show process instances list
    RenderInstanceList();

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

// 3. Update RenderInstanceList in RecipePageUI.cpp
void RecipePageUI::RenderInstanceList() {
  if (!m_processInstances.empty()) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Process Instances (%zu):", m_processInstances.size());

    int indexToRemove = -1;

    for (size_t i = 0; i < m_processInstances.size(); i++) {
      const auto& instance = m_processInstances[i];

      ImGui::PushID(static_cast<int>(i));

      // Highlight selected instance
      bool isSelected = (instance.instanceId == m_selectedInstanceId);
      if (isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.6f, 0.9f, 1.0f));
      }

      // Show nickname if available, otherwise display name
      std::string buttonText = instance.GetUIDisplayName();
      if (ImGui::Button(buttonText.c_str(), ImVec2(-90, 25))) {
        OnInstanceSelected(instance.instanceId);
      }

      if (isSelected) {
        ImGui::PopStyleColor(2);
      }

      // Show tooltip with full info
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("ID: %s", instance.instanceId.c_str());
        ImGui::Text("Type: %s", instance.processType.c_str());
        if (!instance.nickname.empty()) {
          ImGui::Text("Nickname: %s", instance.nickname.c_str());
        }
        ImGui::EndTooltip();
      }

      // Remove button
      ImGui::SameLine();
      if (ImGui::SmallButton("Remove")) {
        indexToRemove = static_cast<int>(i);
      }

      ImGui::PopID();
    }

    // Remove after the loop completes
    if (indexToRemove >= 0) {
      std::string removedId = m_processInstances[indexToRemove].instanceId;
      m_processInstances.erase(m_processInstances.begin() + indexToRemove);

      if (m_selectedInstanceId == removedId) {
        m_selectedInstanceId = "";
      }
      m_isModified = true;
    }
  }
}



void RecipePageUI::RenderRightColumn() {
  if (m_showProcessSelection) {
    RenderProcessSelectionUI();
  }
  else if (!m_selectedInstanceId.empty()) {
    RenderParameterEditor();
  }
  else {
    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.8f, 1.0f), "Recipe Preview");
    ImGui::Separator();
    ImGui::Spacing();

    if (m_newRecipeClicked && strlen(m_recipeNameBuffer) > 0) {
      ImGui::Text("Recipe: %s", m_recipeNameBuffer);
      ImGui::Spacing();

      ImGui::Text("Process Instances (%zu):", m_processInstances.size());
      if (m_processInstances.empty()) {
        ImGui::BulletText("(None yet - click 'Add Process Instance')");
      }
      else {
        for (size_t i = 0; i < m_processInstances.size(); i++) {
          ImGui::BulletText("%zu. %s", i + 1, m_processInstances[i].displayName.c_str());
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

      // Always allow adding processes (removed alreadyAdded check)
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));

      if (ImGui::Button(processName.c_str(), ImVec2(-1, 30))) {
        OnProcessSelected(processName);
      }

      ImGui::PopStyleColor(2);

      // Show tooltip with process info
      if (ImGui::IsItemHovered() && info) {
        ImGui::BeginTooltip();
        ImGui::Text("Category: %s", info->category.c_str());
        ImGui::Text("Description: %s", info->description.c_str());
        ImGui::EndTooltip();
      }

      ImGui::PopID();
      ImGui::Spacing();
    }
  }
  ImGui::EndChild();
}



// 4. Add nickname editor to parameter editor
void RecipePageUI::RenderParameterEditor() {
  ProcessInstance* instance = GetSelectedInstance();
  if (!instance) {
    ImGui::Text("Selected instance not found");
    return;
  }

  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Parameter Editor");
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Text("Instance: %s", instance->GetUIDisplayName().c_str());
  ImGui::Text("Type: %s", instance->processType.c_str());
  ImGui::Text("ID: %s", instance->instanceId.c_str());

  // Add nickname editor at the top
  RenderNicknameEditor(*instance);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Get parameter schema for this process type
  auto schema = ProcessParameterSchema::GetParametersForProcess(instance->processType);

  if (schema.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No parameter schema found for this process type");
    return;
  }

  // Render parameters based on schema
  for (const auto& paramDef : schema) {
    RenderParameterControl(paramDef, *instance);
    ImGui::Spacing();
  }
}

// 5. Implement nickname editor method
void RecipePageUI::RenderNicknameEditor(ProcessInstance& instance) {
  ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Display Name:");

  // Initialize buffer with current nickname
  if (strlen(m_nicknameEditBuffer) == 0 && !instance.nickname.empty()) {
    strncpy(m_nicknameEditBuffer, instance.nickname.c_str(), sizeof(m_nicknameEditBuffer) - 1);
  }

  ImGui::SetNextItemWidth(-80);
  if (ImGui::InputText("##Nickname", m_nicknameEditBuffer, sizeof(m_nicknameEditBuffer))) {
    instance.nickname = std::string(m_nicknameEditBuffer);
    m_isModified = true;
  }

  ImGui::SameLine();
  if (ImGui::Button("Clear")) {
    instance.nickname = "";
    memset(m_nicknameEditBuffer, 0, sizeof(m_nicknameEditBuffer));
    m_isModified = true;
  }

  // Show preview
  if (instance.nickname.empty()) {
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Preview: %s", instance.displayName.c_str());
  }
  else {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Preview: %s", instance.nickname.c_str());
  }
}


void RecipePageUI::RenderParameterControl(const ParameterDefinition& paramDef, ProcessInstance& instance) {
  switch (paramDef.type) {
  case ParameterType::STRING:
    RenderStringParameter(paramDef, instance);
    break;
  case ParameterType::DEVICE_SELECTION:
    RenderDeviceSelectionParameter(paramDef, instance);
    break;
  case ParameterType::NODE_SELECTION:
    RenderNodeSelectionParameter(paramDef, instance);
    break;
  case ParameterType::DOUBLE:
    RenderDoubleParameter(paramDef, instance);
    break;
  case ParameterType::BOOLEAN:
    RenderBooleanParameter(paramDef, instance);
    break;
  }

  // Show description as tooltip
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", paramDef.description.c_str());
  }
}

void RecipePageUI::RenderStringParameter(const ParameterDefinition& paramDef, ProcessInstance& instance) {
  ImGui::Text("%s:", paramDef.name.c_str());
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "%s", instance.parameters[paramDef.name].c_str());
  std::string inputId = "##" + paramDef.name;
  if (ImGui::InputText(inputId.c_str(), buffer, sizeof(buffer))) {
    instance.parameters[paramDef.name] = std::string(buffer);
    m_isModified = true;
  }
}

void RecipePageUI::RenderDoubleParameter(const ParameterDefinition& paramDef, ProcessInstance& instance) {
  ImGui::Text("%s:", paramDef.name.c_str());

  double value = std::stod(instance.parameters[paramDef.name]);
  std::string inputId = "##" + paramDef.name;

  if (ImGui::InputDouble(inputId.c_str(), &value, 0.1, 1.0, "%.1f")) {
    instance.parameters[paramDef.name] = std::to_string(value);
    m_isModified = true;
  }
}

void RecipePageUI::RenderBooleanParameter(const ParameterDefinition& paramDef, ProcessInstance& instance) {
  ImGui::Text("%s:", paramDef.name.c_str());

  bool value = (instance.parameters[paramDef.name] == "true");
  std::string checkboxId = "##" + paramDef.name;

  if (ImGui::Checkbox(checkboxId.c_str(), &value)) {
    instance.parameters[paramDef.name] = value ? "true" : "false";
    m_isModified = true;
  }
}

void RecipePageUI::RenderNodeSelectionParameter(const ParameterDefinition& paramDef, ProcessInstance& instance) {
  ImGui::Text("%s:", paramDef.name.c_str());
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "%s", instance.parameters[paramDef.name].c_str());
  std::string inputId = "##" + paramDef.name;
  if (ImGui::InputText(inputId.c_str(), buffer, sizeof(buffer))) {
    instance.parameters[paramDef.name] = std::string(buffer);
    m_isModified = true;
  }
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[Node]");
}



void RecipePageUI::RenderDeviceSelectionParameter(const ParameterDefinition& paramDef, ProcessInstance& instance) {
  ImGui::Text("%s:", paramDef.name.c_str());
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "%s", instance.parameters[paramDef.name].c_str());
  std::string inputId = "##" + paramDef.name;
  if (ImGui::InputText(inputId.c_str(), buffer, sizeof(buffer))) {
    instance.parameters[paramDef.name] = std::string(buffer);
    m_isModified = true;
  }
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.5f, 1.0f), "[Device]");
}


// Button action methods
void RecipePageUI::OnNewRecipeClicked() {
  m_newRecipeClicked = true;
  m_isModified = false;
  m_showProcessSelection = false;
  ClearRecipeData();
  std::cout << "RecipePageUI: New Recipe clicked" << std::endl;
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
  // Create new process instance
  std::string instanceId = GenerateInstanceId();
  ProcessInstance instance(instanceId, processName);

  // Set default parameters based on process type
  SetDefaultParameters(instance);

  // Add to recipe
  m_processInstances.push_back(instance);
  m_isModified = true;
  m_showProcessSelection = false;

  // Auto-select the new instance for editing
  m_selectedInstanceId = instanceId;

  std::cout << "RecipePageUI: Created instance " << instanceId << " of type " << processName << std::endl;
}

// 6. Update OnInstanceSelected to load nickname into buffer
void RecipePageUI::OnInstanceSelected(const std::string& instanceId) {
  m_selectedInstanceId = instanceId;

  // Load nickname into edit buffer
  ProcessInstance* instance = GetSelectedInstance();
  if (instance) {
    strncpy(m_nicknameEditBuffer, instance->nickname.c_str(), sizeof(m_nicknameEditBuffer) - 1);
    m_nicknameEditBuffer[sizeof(m_nicknameEditBuffer) - 1] = '\0';
  }

  std::cout << "RecipePageUI: Selected instance " << instanceId << " for editing" << std::endl;
}



void RecipePageUI::ClearRecipeData() {
  memset(m_recipeNameBuffer, 0, sizeof(m_recipeNameBuffer));
  m_currentRecipeName.clear();
  m_processInstances.clear();
  m_selectedInstanceId = "";
  // Remove this line: m_nextInstanceNumber = 1;
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

ProcessInstance* RecipePageUI::GetSelectedInstance() {
  for (auto& instance : m_processInstances) {
    if (instance.instanceId == m_selectedInstanceId) {
      return &instance;
    }
  }
  return nullptr;
}

void RecipePageUI::SetDefaultParameters(ProcessInstance& instance) {
  ProcessParameterFactory::InitializeParameters(instance);
}

// Updated implementation
std::string RecipePageUI::GenerateInstanceId() {
  auto now = std::chrono::high_resolution_clock::now();
  auto epoch = now.time_since_epoch();
  auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(epoch);

  std::ostringstream oss;
  oss << "P" << std::hex << std::uppercase << (microseconds.count() & 0xFFFFFFFF);

  return oss.str();
  // Example: P7A4F2B1E (based on current microsecond timestamp)
}


// Create recipes directory if it doesn't exist
void RecipePageUI::EnsureRecipesDirectory() {
  if (!std::filesystem::exists(m_recipesDirectory)) {
    std::filesystem::create_directories(m_recipesDirectory);
  }
}


// 7. Update save/load JSON to include nickname
nlohmann::json RecipePageUI::SerializeRecipe() const {
  nlohmann::json recipeJson;

  // Recipe metadata
  recipeJson["version"] = "1.0";
  recipeJson["created"] = std::time(nullptr);
  recipeJson["name"] = std::string(m_recipeNameBuffer);

  // Process instances array
  nlohmann::json instancesArray = nlohmann::json::array();

  for (const auto& instance : m_processInstances) {
    nlohmann::json instanceJson;
    instanceJson["instanceId"] = instance.instanceId;
    instanceJson["processType"] = instance.processType;
    instanceJson["displayName"] = instance.displayName;
    instanceJson["nickname"] = instance.nickname; // NEW: Save nickname

    // Serialize parameters
    nlohmann::json parametersJson;
    for (const auto& param : instance.parameters) {
      parametersJson[param.first] = param.second;
    }
    instanceJson["parameters"] = parametersJson;

    instancesArray.push_back(instanceJson);
  }

  recipeJson["processInstances"] = instancesArray;
  return recipeJson;
}

// 8. Update deserialize to load nickname
bool RecipePageUI::DeserializeRecipe(const nlohmann::json& recipeJson) {
  try {
    // Clear current recipe
    ClearRecipeData();

    // Load recipe name
    if (recipeJson.contains("name")) {
      std::string recipeName = recipeJson["name"];
      strncpy(m_recipeNameBuffer, recipeName.c_str(), sizeof(m_recipeNameBuffer) - 1);
      m_currentRecipeName = recipeName;
    }

    // Load process instances
    if (recipeJson.contains("processInstances") && recipeJson["processInstances"].is_array()) {
      for (const auto& instanceJson : recipeJson["processInstances"]) {
        ProcessInstance instance(
          instanceJson["instanceId"],
          instanceJson["processType"]
        );

        // Load nickname if present
        if (instanceJson.contains("nickname")) {
          instance.nickname = instanceJson["nickname"];
        }

        // Load parameters
        if (instanceJson.contains("parameters")) {
          for (const auto& param : instanceJson["parameters"].items()) {
            instance.parameters[param.key()] = param.value();
          }
        }

        m_processInstances.push_back(instance);
      }
    }

    m_newRecipeClicked = true;
    m_isModified = false;
    return true;

  }
  catch (const std::exception& e) {
    std::cout << "Error deserializing recipe: " << e.what() << std::endl;
    return false;
  }
}


// Save recipe to file
bool RecipePageUI::SaveRecipeToFile(const std::string& filename) {
  try {
    EnsureRecipesDirectory();

    nlohmann::json recipeJson = SerializeRecipe();

    std::string filepath = m_recipesDirectory + filename + ".json";
    std::ofstream file(filepath);

    if (!file.is_open()) {
      std::cout << "Failed to open file for writing: " << filepath << std::endl;
      return false;
    }

    file << recipeJson.dump(2); // Pretty print with 2-space indent
    file.close();

    std::cout << "Recipe saved to: " << filepath << std::endl;
    return true;

  }
  catch (const std::exception& e) {
    std::cout << "Error saving recipe: " << e.what() << std::endl;
    return false;
  }
}

// Load recipe from file
bool RecipePageUI::LoadRecipeFromFile(const std::string& filename) {
  try {
    std::string filepath = m_recipesDirectory + filename + ".json";

    if (!std::filesystem::exists(filepath)) {
      std::cout << "Recipe file not found: " << filepath << std::endl;
      return false;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
      std::cout << "Failed to open file for reading: " << filepath << std::endl;
      return false;
    }

    nlohmann::json recipeJson;
    file >> recipeJson;
    file.close();

    bool success = DeserializeRecipe(recipeJson);
    if (success) {
      std::cout << "Recipe loaded from: " << filepath << std::endl;
    }

    return success;

  }
  catch (const std::exception& e) {
    std::cout << "Error loading recipe: " << e.what() << std::endl;
    return false;
  }
}

// Get list of available recipe files
std::vector<std::string> RecipePageUI::GetAvailableRecipes() const {
  std::vector<std::string> recipes;

  if (!std::filesystem::exists(m_recipesDirectory)) {
    return recipes;
  }

  for (const auto& entry : std::filesystem::directory_iterator(m_recipesDirectory)) {
    if (entry.is_regular_file() && entry.path().extension() == ".json") {
      std::string filename = entry.path().stem().string();
      recipes.push_back(filename);
    }
  }

  std::sort(recipes.begin(), recipes.end());
  return recipes;
}

// Updated save method
void RecipePageUI::OnSaveRecipeClicked() {
  if (ValidateRecipeName()) {
    std::string recipeName = std::string(m_recipeNameBuffer);

    if (SaveRecipeToFile(recipeName)) {
      m_currentRecipeName = recipeName;
      m_isModified = false;
      std::cout << "Recipe '" << recipeName << "' saved successfully" << std::endl;
      std::cout << "Process instances: " << m_processInstances.size() << std::endl;

      // Print summary
      for (const auto& instance : m_processInstances) {
        std::cout << "  " << instance.displayName << " (" << instance.instanceId << ")" << std::endl;
      }
    }
    else {
      std::cout << "Failed to save recipe: " << recipeName << std::endl;
    }
  }
}

// Fixed Load dialog implementation
void RecipePageUI::ShowLoadRecipeDialog() {
  static bool showDialog = false;  // Changed to false
  static std::string selectedRecipe = "";

  // This will open the dialog when called
  if (!showDialog) {
    showDialog = true;
    ImGui::OpenPopup("Load Recipe");
  }

  if (ImGui::BeginPopupModal("Load Recipe", &showDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Select a recipe to load:");
    ImGui::Separator();

    auto availableRecipes = GetAvailableRecipes();

    if (availableRecipes.empty()) {
      ImGui::Text("No saved recipes found.");
    }
    else {
      for (const auto& recipe : availableRecipes) {
        if (ImGui::Selectable(recipe.c_str(), selectedRecipe == recipe)) {
          selectedRecipe = recipe;
        }
      }
    }

    ImGui::Separator();

    bool canLoad = !selectedRecipe.empty();
    if (!canLoad) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }

    if (ImGui::Button("Load") && canLoad) {
      if (LoadRecipeFromFile(selectedRecipe)) {
        showDialog = false;
        selectedRecipe = "";  // Reset selection
      }
    }

    if (!canLoad) {
      ImGui::PopStyleVar();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      showDialog = false;
      selectedRecipe = "";  // Reset selection
    }

    ImGui::EndPopup();
  }
}
// Updated load method
void RecipePageUI::OnLoadRecipeClicked() {
  ShowLoadRecipeDialog();
}