#pragma once
#include "imgui.h"
#include "ProcessRegistry.h"
#include "ProcessParameterSchema.h"
#include "ProcessInstance.h"  // Include the separate header instead of defining it here

#include <string>
#include <vector>
#include <map>

// Add to RecipePageUI.h
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>


//// Structure to hold process instance data
//struct ProcessInstance {
//  std::string instanceId;      // e.g., "P0001"
//  std::string processType;     // e.g., "Core_PickPlace"
//  std::string displayName;     // e.g., "CorePickPlace_P0001"
//
//  // Parameters for the process
//  std::map<std::string, std::string> parameters;
//
//  ProcessInstance(const std::string& id, const std::string& type)
//    : instanceId(id), processType(type) {
//    displayName = type + "_" + id;
//  }
//};

class RecipePageUI {
public:
  RecipePageUI();
  ~RecipePageUI();

  void RenderUI();

  // Getters for state
  bool IsRecipeModified() const { return m_isModified; }
  const std::string& GetCurrentRecipeName() const { return m_currentRecipeName; }

private:
  // UI State
  bool m_newRecipeClicked = false;
  bool m_isModified = false;
  char m_recipeNameBuffer[256] = "";
  std::string m_currentRecipeName = "";

  // Process selection and instance management
  bool m_showProcessSelection = false;
  std::string m_selectedCategory = "All";
  std::vector<ProcessInstance> m_processInstances;
  std::string m_selectedInstanceId = "";  // Currently selected instance for editing

  // Instance ID generation
  int m_nextInstanceNumber = 1;
  std::string GenerateInstanceId();

  // Layout constants
  static constexpr float LEFT_COLUMN_WIDTH_RATIO = 0.25f;
  static constexpr float CENTER_COLUMN_WIDTH_RATIO = 0.25f;
  static constexpr float RIGHT_COLUMN_WIDTH_RATIO = 0.50f;

  // Column render methods
  void RenderLeftColumn();
  void RenderCenterColumn();
  void RenderRightColumn();

  // Button action methods
  void OnNewRecipeClicked();
  void OnLoadRecipeClicked();
  void OnSaveRecipeClicked();
  void OnAddProcessInstanceClicked();

  // Process selection and instance management
  void OnProcessSelected(const std::string& processName);
  void OnInstanceSelected(const std::string& instanceId);
  void RenderProcessSelectionUI();
  void RenderProcessList(const std::vector<std::string>& processes);
  void RenderInstanceList();

  // Parameter editing system
  void RenderParameterEditor();
  void RenderParameterControl(const ParameterDefinition& paramDef, ProcessInstance& instance);
  void RenderStringParameter(const ParameterDefinition& paramDef, ProcessInstance& instance);
  void RenderDoubleParameter(const ParameterDefinition& paramDef, ProcessInstance& instance);
  void RenderBooleanParameter(const ParameterDefinition& paramDef, ProcessInstance& instance);
  void RenderNodeSelectionParameter(const ParameterDefinition& paramDef, ProcessInstance& instance);
  void RenderDeviceSelectionParameter(const ParameterDefinition& paramDef, ProcessInstance& instance);

  // Helper methods
  void ClearRecipeData();
  bool ValidateRecipeName() const;
  ProcessInstance* GetSelectedInstance();
  void SetDefaultParameters(ProcessInstance& instance);

  std::string m_recipesDirectory = "recipes/";
  void EnsureRecipesDirectory();
  // Add these new methods:
  bool SaveRecipeToFile(const std::string& filename);
  bool LoadRecipeFromFile(const std::string& filename);
  nlohmann::json SerializeRecipe() const;
  bool DeserializeRecipe(const nlohmann::json& recipeJson);
  std::vector<std::string> GetAvailableRecipes() const;
  void ShowLoadRecipeDialog();
  // Add this for nickname editing
  char m_nicknameEditBuffer[128] = "";

  // Add method declaration
  void RenderNicknameEditor(ProcessInstance& instance);
};