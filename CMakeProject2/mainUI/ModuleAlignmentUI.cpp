// ModuleAlignmentUI.cpp - Implementation with ProductReference integration
#include "ModuleAlignmentUI.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <filesystem>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// CONSTRUCTION & LIFECYCLE
// =============================================================================

ModuleAlignmentUI::ModuleAlignmentUI() {
  m_moduleAlignment = std::make_unique<ModuleAlignment>();
  RefreshSavedAlignments();

  // Initialize ProductReference file path
  strcpy_s(m_productFilePath, "products/");
}

ModuleAlignmentUI::~ModuleAlignmentUI() {
  // Ensure any running alignment is completed before destruction
  if (IsAlignmentRunning()) {
    // Wait for completion or timeout
    if (m_alignmentFuture.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
      std::cerr << "ModuleAlignmentUI: Warning - Alignment still running during destruction" << std::endl;
    }
  }
}

void ModuleAlignmentUI::SetMachineOperations(MachineOperations* machineOps) {
  if (m_moduleAlignment) {
    m_moduleAlignment->SetMachineOperations(machineOps);
  }
}

void ModuleAlignmentUI::SetCameraManager(CameraManager* cameraManager) {
  if (m_moduleAlignment) {
    m_moduleAlignment->SetCameraManager(cameraManager);
  }
}

// =============================================================================
// MAIN UI RENDERING
// =============================================================================

void ModuleAlignmentUI::RenderUI() {
  if (!m_showWindow) return;

  // Update progress if alignment is running
  bool shouldUpdateProgress = (m_currentState == AlignmentState::RUNNING) && m_alignmentFuture.valid();
  if (shouldUpdateProgress) {
    UpdateProgress();
  }

  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Module Alignment System", &m_showWindow)) {
    ImGui::End();
    return;
  }

  // Status bar at top
  ImVec4 stateColor = GetStateColor(m_currentState);
  ImGui::TextColored(stateColor, "Status: %s", GetStateText(m_currentState));
  ImGui::SameLine();

  // Ready indicator
  bool isReady = m_moduleAlignment && m_moduleAlignment->IsReadyForAlignment();
  ImGui::TextColored(isReady ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
    "Ready: %s", isReady ? "YES" : "NO");

  if (!isReady) {
    ImGui::SameLine();
    ImGui::TextDisabled("(Set MachineOperations first)");
  }

  // Show prominent results notification
  if (m_hasResult && m_lastResult.success) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Results Available!");
  }
  else if (m_hasResult && !m_lastResult.success) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ Alignment Failed");
  }

  ImGui::Separator();

  // Main content area with tabs
  if (ImGui::BeginTabBar("AlignmentTabs")) {

    // Configuration Tab
    if (ImGui::BeginTabItem("Configuration", nullptr, (m_activeTab == 0) ? ImGuiTabItemFlags_SetSelected : 0)) {
      if (ImGui::IsItemActive()) m_activeTab = 0;
      RenderAlignmentConfiguration();
      ImGui::EndTabItem();
    }

    // Results Tab - Highlight when results are available
    ImGuiTabItemFlags resultsFlags = 0;
    if (m_activeTab == 1) resultsFlags |= ImGuiTabItemFlags_SetSelected;
    if (m_hasResult && m_lastResult.success) resultsFlags |= ImGuiTabItemFlags_UnsavedDocument;

    if (ImGui::BeginTabItem("Results", nullptr, resultsFlags)) {
      if (ImGui::IsItemActive()) m_activeTab = 1;
      RenderResultsSection();
      ImGui::EndTabItem();
    }

    // Other tabs...
    if (ImGui::BeginTabItem("Saved Alignments", nullptr, (m_activeTab == 2) ? ImGuiTabItemFlags_SetSelected : 0)) {
      if (ImGui::IsItemActive()) m_activeTab = 2;
      RenderSavedAlignmentsSection();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Transform Test", nullptr, (m_activeTab == 3) ? ImGuiTabItemFlags_SetSelected : 0)) {
      if (ImGui::IsItemActive()) m_activeTab = 3;
      RenderTransformationTestSection();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Advanced", nullptr, (m_activeTab == 4) ? ImGuiTabItemFlags_SetSelected : 0)) {
      if (ImGui::IsItemActive()) m_activeTab = 4;
      RenderAdvancedSettings();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  // Progress section (always visible when running)
  if (m_currentState == AlignmentState::RUNNING) {
    ImGui::Separator();
    RenderProgressSection();
  }

  // Dialogs
  if (m_showSaveDialog) RenderSaveDialog();
  if (m_showLoadDialog) RenderLoadDialog();
  if (m_showDeleteDialog) RenderDeleteConfirmDialog();

  ImGui::End();
}

// =============================================================================
// UI SECTION RENDERING
// =============================================================================

void ModuleAlignmentUI::RenderAlignmentConfiguration() {
  ImGui::Text("3-Point Alignment Configuration");
  ImGui::Separator();

  // ADD: ProductReference file section
  RenderProductFileSection();

  ImGui::Separator();

  // Node configuration
  ImGui::Text("Calibration Nodes:");
  ImGui::InputText("Node 1 (Origin)", m_node1Name, sizeof(m_node1Name));
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("First node serves as origin reference point");
  }

  ImGui::InputText("Node 2 (X-axis)", m_node2Name, sizeof(m_node2Name));
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Second node defines X-axis direction (Node1 → Node2)");
  }

  ImGui::InputText("Node 3 (Y-axis)", m_node3Name, sizeof(m_node3Name));
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Third node defines Y-axis direction (Node1 → Node3)");
  }

  ImGui::Spacing();

  // Z coordinate option
  ImGui::Checkbox("Use Robot Z coordinates", &m_useRobotZ);
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("If checked, uses robot Z position instead of vision-detected Z");
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Start alignment button
  bool canStart = m_moduleAlignment && m_moduleAlignment->IsReadyForAlignment() &&
    !IsAlignmentRunning() &&
    strlen(m_node1Name) > 0 && strlen(m_node2Name) > 0 && strlen(m_node3Name) > 0;

  if (!canStart) {
    ImGui::BeginDisabled();
  }

  if (ImGui::Button("Start 3-Point Alignment", ImVec2(200, 40))) {
    StartAlignment();
  }

  if (!canStart) {
    ImGui::EndDisabled();

    // Show why button is disabled
    if (!m_moduleAlignment || !m_moduleAlignment->IsReadyForAlignment()) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Please set MachineOperations first");
    }
    else if (strlen(m_node1Name) == 0 || strlen(m_node2Name) == 0 || strlen(m_node3Name) == 0) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Please specify all three node names");
    }
    else if (IsAlignmentRunning()) {
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "Alignment in progress...");
    }
  }

  ImGui::SameLine();

  // Reset button
  if (ImGui::Button("Reset", ImVec2(80, 40))) {
    ResetAlignment();
  }

  // Quick preset buttons
  ImGui::Spacing();
  ImGui::Text("Quick Presets:");

  if (ImGui::Button("Default Nodes")) {
    strcpy_s(m_node1Name, "node_fid1");
    strcpy_s(m_node2Name, "node_fid2");
    strcpy_s(m_node3Name, "node_fid3");
  }
  ImGui::SameLine();

  if (ImGui::Button("Test Nodes")) {
    strcpy_s(m_node1Name, "node_fid1");
    strcpy_s(m_node2Name, "node_fid2");
    strcpy_s(m_node3Name, "node_fid3");
  }
}

// =============================================================================
// ADD: PRODUCTREFERENCE FILE SECTION
// =============================================================================

void ModuleAlignmentUI::RenderProductFileSection() {
  ImGui::Text("📁 ProductReference Files");

  // Toggle section visibility
  if (ImGui::Button(m_showProductFileSection ? "Hide Files ▼" : "Show Files ▶")) {
    m_showProductFileSection = !m_showProductFileSection;
    if (m_showProductFileSection) {
      RefreshProductFileList();
      RefreshLoadedProducts();
    }
  }

  if (!m_showProductFileSection) {
    return;
  }

  ImGui::Indent();

  // Directory path
  ImGui::Text("Directory:");
  ImGui::SameLine();
  ImGui::InputText("##ProductDir", m_productFilePath, sizeof(m_productFilePath));
  ImGui::SameLine();
  if (ImGui::Button("Scan")) {
    RefreshProductFileList();
  }

  // Available files
  ImGui::Text("Available Files:");
  if (m_availableProductFiles.empty()) {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "No .json files found in directory");
  }
  else {
    if (ImGui::BeginListBox("##ProductFiles", ImVec2(-1, 100))) {
      for (size_t i = 0; i < m_availableProductFiles.size(); i++) {
        if (ImGui::Selectable(m_availableProductFiles[i].c_str())) {
          // FIX: Pass just the filename, not the full path
          std::string fileName = m_availableProductFiles[i]; // Just filename, no "products/" prefix
          std::cout << "Loading file: " << fileName << std::endl;
          LoadProductFile(fileName);
        }
      }
      ImGui::EndListBox();
    }
  }

  // Loaded products section
  ImGui::Spacing();
  RenderProductSelection();

  ImGui::Unindent();
}

void ModuleAlignmentUI::RenderProductSelection() {
  ImGui::Text("Loaded Products:");


  // DEBUG: Add status check
  if (!m_productReferenceManager) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "ProductReferenceManager not set");
    std::cout << "DEBUG: ProductReferenceManager is null in RenderProductSelection" << std::endl;
    return;
  }

  // DEBUG: Show product count
  const auto& allProducts = m_productReferenceManager->GetAllProductReferences();
  ImGui::Text("DEBUG: Manager has %zu products", allProducts.size());


  if (!m_productReferenceManager) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "ProductReferenceManager not set");
    return;
  }

  if (m_loadedProducts.empty()) {
    // DEBUG: Add manual refresh button
    if (ImGui::Button("DEBUG: Manual Refresh")) {
      std::cout << "Manual refresh triggered" << std::endl;
      RefreshLoadedProducts();
    }
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "No products loaded");
    return;
  }

  // Product selection dropdown
  if (ImGui::BeginCombo("##LoadedProducts",
    m_selectedProductIndex >= 0 ? m_loadedProducts[m_selectedProductIndex].c_str() : "Select Product...")) {

    for (int i = 0; i < static_cast<int>(m_loadedProducts.size()); i++) {
      bool isSelected = (i == m_selectedProductIndex);
      if (ImGui::Selectable(m_loadedProducts[i].c_str(), isSelected)) {
        m_selectedProductIndex = i;
        m_selectedProductName = m_loadedProducts[i];
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Apply button
  if (m_selectedProductIndex >= 0) {
    ImGui::SameLine();
    if (ImGui::Button("Apply Nodes")) {
      ApplyProductNodesToAlignment(m_selectedProductName);
    }

    // Show product info
    if (m_productReferenceManager) {
      const auto* product = m_productReferenceManager->GetProductReference(m_selectedProductName);
      if (product) {
        ImGui::Text("Product: %s", product->name.c_str());
        if (!product->id.empty()) {
          ImGui::SameLine();
          ImGui::TextColored(ImVec4(0, 1, 0, 1), "ID: %s", product->id.c_str());
        }
        ImGui::Text("Points: %zu", product->points.size());
      }
    }
  }
}

// =============================================================================
// PRODUCTREFERENCE HELPER METHODS
// =============================================================================

void ModuleAlignmentUI::RefreshProductFileList() {
  m_availableProductFiles.clear();

  try {
    std::string dirPath = m_productFilePath;
    if (std::filesystem::exists(dirPath) && std::filesystem::is_directory(dirPath)) {
      for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
          m_availableProductFiles.push_back(entry.path().filename().string());
        }
      }
      std::sort(m_availableProductFiles.begin(), m_availableProductFiles.end());
    }
  }
  catch (const std::exception& e) {
    std::cout << "Error scanning directory: " << e.what() << std::endl;
  }
}



void ModuleAlignmentUI::LoadProductFile(const std::string& filePath) {
  std::cout << "=== LoadProductFile called with: " << filePath << std::endl;

  if (!m_productReferenceManager) {
    std::cout << "ERROR: ProductReferenceManager is null!" << std::endl;
    return;
  }

  try {
    // FIX: Remove the directory prefix since ProductReferenceManager adds it internally
    std::string fileName = filePath;

    // If filePath starts with "products/", remove it
    if (fileName.substr(0, 9) == "products/") {
      fileName = fileName.substr(9); // Remove "products/" prefix
      std::cout << "Removed prefix, using filename: " << fileName << std::endl;
    }

    std::cout << "Attempting to load file: " << fileName << std::endl;
    bool loadResult = m_productReferenceManager->LoadFromFile(fileName);

    std::cout << "LoadFromFile result: " << (loadResult ? "SUCCESS" : "FAILED") << std::endl;

    if (loadResult) {
      std::cout << "Successfully loaded: " << fileName << std::endl;
      RefreshLoadedProducts();

      // Debug: Check what products are now loaded
      const auto& allProducts = m_productReferenceManager->GetAllProductReferences();
      std::cout << "Total products loaded: " << allProducts.size() << std::endl;
      for (const auto& product : allProducts) {
        std::cout << "  - Product: " << product.name << " (ID: " << product.id << ")" << std::endl;
      }
    }
    else {
      std::cout << "Failed to load: " << fileName << std::endl;
    }
  }
  catch (const std::exception& e) {
    std::cout << "Exception loading file: " << e.what() << std::endl;
  }
}


// Also add debugging to RefreshLoadedProducts method
void ModuleAlignmentUI::RefreshLoadedProducts() {
  std::cout << "=== RefreshLoadedProducts called" << std::endl;

  m_loadedProducts.clear();
  m_selectedProductIndex = -1;

  if (!m_productReferenceManager) {
    std::cout << "ERROR: ProductReferenceManager is null in RefreshLoadedProducts!" << std::endl;
    return;
  }

  const auto& allProducts = m_productReferenceManager->GetAllProductReferences();
  std::cout << "Found " << allProducts.size() << " products in manager" << std::endl;

  for (const auto& product : allProducts) {
    std::cout << "Adding product to list: " << product.name << std::endl;
    m_loadedProducts.push_back(product.name);
  }

  std::cout << "m_loadedProducts now has " << m_loadedProducts.size() << " entries" << std::endl;
}


void ModuleAlignmentUI::ApplyProductNodesToAlignment(const std::string& productName) {
  std::string node1, node2, node3;

  if (GetProductFiducialNodes(productName, node1, node2, node3)) {
    strcpy_s(m_node1Name, node1.c_str());
    strcpy_s(m_node2Name, node2.c_str());
    strcpy_s(m_node3Name, node3.c_str());

    std::cout << "Applied nodes from " << productName << ": "
      << node1 << ", " << node2 << ", " << node3 << std::endl;
  }
  else {
    std::cout << "Failed to get fiducial nodes from " << productName << std::endl;
  }
}

bool ModuleAlignmentUI::GetProductFiducialNodes(const std::string& productName,
  std::string& node1, std::string& node2, std::string& node3) {
  if (!m_productReferenceManager) {
    return false;
  }

  const auto* product = m_productReferenceManager->GetProductReference(productName);
  if (!product) {
    return false;
  }

  // Check if product has 3-point datum system
  if (product->datum.constructionMethod == ProductReferenceManager::DatumReference::ConstructionMethod::THREE_POINT) {
    // Get nodes from datum system
    node1 = product->datum.originPointName;

    // Get second point from X-axis edge
    const auto* xAxisEdge = m_productReferenceManager->GetEdge(productName, product->datum.xAxisEdgeName);
    if (xAxisEdge) {
      node2 = (xAxisEdge->point1Name == node1) ? xAxisEdge->point2Name : xAxisEdge->point1Name;
    }

    // Get third point from Y-axis edge
    const auto* yAxisEdge = m_productReferenceManager->GetEdge(productName, product->datum.yAxisEdgeName);
    if (yAxisEdge) {
      node3 = (yAxisEdge->point1Name == node1) ? yAxisEdge->point2Name : yAxisEdge->point1Name;
    }

    return !node1.empty() && !node2.empty() && !node3.empty();
  }
  else {
    // Fallback: find first 3 fiducial points
    std::vector<std::string> fiducialNodes;
    for (const auto& point : product->points) {
      if (point.actionType == ProductReferenceManager::Point3D::ActionType::FIDUCIAL) {
        fiducialNodes.push_back(point.name);
        if (fiducialNodes.size() >= 3) break;
      }
    }

    if (fiducialNodes.size() >= 3) {
      node1 = fiducialNodes[0];
      node2 = fiducialNodes[1];
      node3 = fiducialNodes[2];
      return true;
    }
  }

  return false;
}

// =============================================================================
// EXISTING UI SECTIONS (Unchanged from baseline)
// =============================================================================

void ModuleAlignmentUI::RenderProgressSection() {
  ImGui::Text("Alignment Progress");
  ImGui::Separator();

  // Progress bar
  ImGui::ProgressBar(m_progressValue, ImVec2(-1.0f, 0.0f));

  // Current step
  if (!m_currentStep.empty()) {
    ImGui::Text("Current Step: %s", m_currentStep.c_str());
  }

  // Status message
  if (!m_statusMessage.empty()) {
    ImGui::TextWrapped("Status: %s", m_statusMessage.c_str());
  }

  // Cancel button
  if (ImGui::Button("Cancel Alignment")) {
    ResetAlignment();
  }
}

void ModuleAlignmentUI::RenderResultsSection() {
  if (!m_hasResult) {
    ImGui::Text("No alignment results available.");
    ImGui::Text("Run a 3-point alignment to see results here.");
    return;
  }

  const auto& result = m_lastResult;

  // Large, prominent success/failure indicator
  if (result.success) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "🎯 ALIGNMENT SUCCESSFUL");
  }
  else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "❌ ALIGNMENT FAILED");
  }

  if (!result.success) {
    if (!result.errorMessage.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "Error Details:");
      ImGui::TextWrapped("%s", result.errorMessage.c_str());
    }
    return;
  }

  ImGui::Separator();

  // Key Results
  ImGui::Text("📍 CENTER POSITION:");
  ImGui::Indent();
  ImGui::Text("X: %.4f mm", result.centerPosition.x);
  ImGui::Text("Y: %.4f mm", result.centerPosition.y);
  ImGui::Text("Z: %.4f mm", result.centerPosition.z);
  ImGui::Unindent();

  ImGui::Spacing();
  ImGui::Text("📏 AXIS MEASUREMENTS:");
  ImGui::Indent();
  ImGui::Text("X-axis length: %.4f mm", result.xAxisLength);
  ImGui::Text("Y-axis length: %.4f mm", result.yAxisLength);
  double axisAngleBetween = CalculateAngleBetweenVectors(result.xAxisDirection, result.yAxisDirection);
  ImGui::Text("Angle between axes: %.2f° (should be ~90°)", axisAngleBetween);
  ImGui::Text("Rotation from global X: %.2f°", result.axisAngle);
  ImGui::Unindent();

  // Action buttons
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("💾 Actions:");
  if (ImGui::Button("Save This Alignment", ImVec2(150, 30))) {
    m_showSaveDialog = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Test Coordinates", ImVec2(130, 30))) {
    m_activeTab = 3; // Switch to Transform Test tab
  }
  ImGui::SameLine();
  if (ImGui::Button("New Alignment", ImVec2(120, 30))) {
    ResetAlignment();
    m_activeTab = 0; // Switch back to Configuration tab
  }
}

void ModuleAlignmentUI::RenderSavedAlignmentsSection() {
  ImGui::Text("Saved Alignment Configurations");
  ImGui::Separator();

  // Refresh button
  if (ImGui::Button("Refresh List")) {
    RefreshSavedAlignments();
  }
  ImGui::SameLine();

  if (ImGui::Button("Load Selected") && m_selectedAlignment >= 0 &&
    m_selectedAlignment < static_cast<int>(m_savedAlignments.size())) {
    m_showLoadDialog = true;
  }
  ImGui::SameLine();

  if (ImGui::Button("Delete Selected") && m_selectedAlignment >= 0 &&
    m_selectedAlignment < static_cast<int>(m_savedAlignments.size())) {
    m_showDeleteDialog = true;
  }

  ImGui::Spacing();

  // List of saved alignments
  if (m_savedAlignments.empty()) {
    ImGui::Text("No saved alignments found.");
  }
  else {
    ImGui::Text("Available Alignments:");

    if (ImGui::BeginListBox("##SavedAlignments", ImVec2(-1, 200))) {
      for (int i = 0; i < static_cast<int>(m_savedAlignments.size()); i++) {
        bool isSelected = (i == m_selectedAlignment);
        if (ImGui::Selectable(m_savedAlignments[i].c_str(), isSelected)) {
          m_selectedAlignment = i;
        }
      }
      ImGui::EndListBox();
    }
  }
}

void ModuleAlignmentUI::RenderTransformationTestSection() {
  ImGui::Text("Coordinate Transformation Testing");
  ImGui::Separator();

  if (!m_moduleAlignment || !m_moduleAlignment->HasValidAlignment()) {
    ImGui::Text("No valid alignment available for transformation testing.");
    ImGui::Text("Complete an alignment first or load a saved alignment.");
    return;
  }

  // Move to Local Coordinate section
  ImGui::Text("🎯 Move to Local Coordinate");
  ImGui::Separator();

  // Input fields for target local position
  ImGui::Text("Target Local Position (mm):");
  ImGui::InputFloat3("##TargetLocalPos", m_targetLocalPos);

  // Device selection
  ImGui::Text("Device:");
  ImGui::SameLine();
  ImGui::InputText("##DeviceName", m_deviceName, sizeof(m_deviceName));

  // Wait for completion option
  ImGui::Checkbox("Wait for completion", &m_waitForCompletion);
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("If checked, waits for movement to complete before returning");
  }

  // Move button
  bool canMove = m_moduleAlignment && m_moduleAlignment->HasValidAlignment() &&
    m_moduleAlignment->IsReadyForAlignment();

  if (!canMove) {
    ImGui::BeginDisabled();
  }

  if (ImGui::Button("Move to Local Position", ImVec2(180, 30))) {
    ExecuteMoveToLocal();
  }

  if (!canMove) {
    ImGui::EndDisabled();
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "System not ready for movement");
  }

  // Quick position buttons
  ImGui::Spacing();
  ImGui::Text("Quick Positions:");

  if (ImGui::Button("Go to Center (0,0,0)", ImVec2(140, 25))) {
    m_targetLocalPos[0] = 0.0f;
    m_targetLocalPos[1] = 0.0f;
    m_targetLocalPos[2] = 0.0f;
    ExecuteMoveToLocal();
  }

  ImGui::SameLine();
  if (ImGui::Button("Move +5mm X", ImVec2(100, 25))) {
    m_targetLocalPos[0] += 5.0f;
    ExecuteMoveToLocal();
  }

  ImGui::SameLine();
  if (ImGui::Button("Move -5mm X", ImVec2(100, 25))) {
    m_targetLocalPos[0] -= 5.0f;
    ExecuteMoveToLocal();
  }

  if (ImGui::Button("Move +5mm Y", ImVec2(100, 25))) {
    m_targetLocalPos[1] += 5.0f;
    ExecuteMoveToLocal();
  }

  ImGui::SameLine();
  if (ImGui::Button("Move -5mm Y", ImVec2(100, 25))) {
    m_targetLocalPos[1] -= 5.0f;
    ExecuteMoveToLocal();
  }

  ImGui::SameLine();
  if (ImGui::Button("Get Current Position", ImVec2(150, 25))) {
    GetCurrentLocalPosition();
  }

  // Show current local position
  ImGui::Spacing();
  ImGui::Text("Current Local Position:");
  ImGui::InputFloat3("##CurrentLocalPos", m_currentLocalPos);
  ImGui::SameLine();
  ImGui::TextDisabled("(Read-only)");

  // Movement status
  if (!m_lastMoveStatus.empty()) {
    ImVec4 statusColor = m_lastMoveSuccess ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);
    ImGui::TextColored(statusColor, "Last Move: %s", m_lastMoveStatus.c_str());
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Coordinate transformation testing
  ImGui::Text("📐 Coordinate Transformation Testing");
  ImGui::Separator();

  // Machine to Alignment transformation
  ImGui::Text("Machine → Alignment Coordinates");
  ImGui::InputFloat3("Machine Position (mm)", m_testMachinePos);

  if (ImGui::Button("Transform to Alignment")) {
    PositionStruct machinePos = { m_testMachinePos[0], m_testMachinePos[1], m_testMachinePos[2], 0, 0, 0 };
    PositionStruct alignmentPos;

    if (m_moduleAlignment->TransformMachineToAlignment(machinePos, alignmentPos)) {
      m_testAlignmentPos[0] = static_cast<float>(alignmentPos.x);
      m_testAlignmentPos[1] = static_cast<float>(alignmentPos.y);
      m_testAlignmentPos[2] = static_cast<float>(alignmentPos.z);
    }
  }

  ImGui::InputFloat3("Alignment Position (mm)", m_testAlignmentPos);

  // Alignment to Machine transformation
  ImGui::Spacing();
  if (ImGui::Button("Transform to Machine")) {
    PositionStruct alignmentPos = { m_testAlignmentPos[0], m_testAlignmentPos[1], m_testAlignmentPos[2], 0, 0, 0 };
    PositionStruct machinePos;

    if (m_moduleAlignment->TransformAlignmentToMachine(alignmentPos, machinePos)) {
      m_testMachinePos[0] = static_cast<float>(machinePos.x);
      m_testMachinePos[1] = static_cast<float>(machinePos.y);
      m_testMachinePos[2] = static_cast<float>(machinePos.z);
    }
  }

  // Quick test buttons
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Quick Tests:");

  if (ImGui::Button("Test Origin (0,0,0)")) {
    m_testAlignmentPos[0] = 0.0f;
    m_testAlignmentPos[1] = 0.0f;
    m_testAlignmentPos[2] = 0.0f;

    PositionStruct alignmentPos = { 0, 0, 0, 0, 0, 0 };
    PositionStruct machinePos;

    if (m_moduleAlignment->TransformAlignmentToMachine(alignmentPos, machinePos)) {
      m_testMachinePos[0] = static_cast<float>(machinePos.x);
      m_testMachinePos[1] = static_cast<float>(machinePos.y);
      m_testMachinePos[2] = static_cast<float>(machinePos.z);
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Test X-axis (10,0,0)")) {
    m_testAlignmentPos[0] = 10.0f;
    m_testAlignmentPos[1] = 0.0f;
    m_testAlignmentPos[2] = 0.0f;
  }

  ImGui::SameLine();
  if (ImGui::Button("Test Y-axis (0,10,0)")) {
    m_testAlignmentPos[0] = 0.0f;
    m_testAlignmentPos[1] = 10.0f;
    m_testAlignmentPos[2] = 0.0f;
  }
}

void ModuleAlignmentUI::RenderAdvancedSettings() {
  ImGui::Text("Advanced Settings & Diagnostics");
  ImGui::Separator();

  // System status
  ImGui::Text("System Status:");
  bool hasModuleAlignment = (m_moduleAlignment != nullptr);
  bool hasValidAlignment = hasModuleAlignment && m_moduleAlignment->HasValidAlignment();
  bool isReady = hasModuleAlignment && m_moduleAlignment->IsReadyForAlignment();

  ImGui::BulletText("ModuleAlignment: %s", hasModuleAlignment ? "OK" : "Missing");
  ImGui::BulletText("Ready for Alignment: %s", isReady ? "YES" : "NO");
  ImGui::BulletText("Has Valid Alignment: %s", hasValidAlignment ? "YES" : "NO");

  if (hasModuleAlignment) {
    std::string lastError = m_moduleAlignment->GetLastError();
    if (!lastError.empty()) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Last Error: %s", lastError.c_str());
    }
  }

  ImGui::Spacing();
  ImGui::Separator();

  // ProductReference directory setting
  ImGui::Text("ProductReference Settings:");
  ImGui::InputText("Products Directory", m_productFilePath, sizeof(m_productFilePath));
  ImGui::SameLine();
  if (ImGui::Button("Scan Directory")) {
    RefreshProductFileList();
  }

  // Mock detection option
  static bool useMockDetection = false;
  ImGui::Checkbox("Use Mock Detection (for testing)", &useMockDetection);
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("When enabled, uses simulated detection results instead of real camera");
  }

  // Database operations
  ImGui::Spacing();
  ImGui::Text("Database Operations:");

  if (ImGui::Button("Clear All Saved Alignments")) {
    RefreshSavedAlignments();
  }

  // Diagnostics
  ImGui::Spacing();
  ImGui::Text("Diagnostics:");

  if (ImGui::Button("Test Movement Only") && isReady) {
    ImGui::OpenPopup("Movement Test");
  }

  // Movement test popup
  if (ImGui::BeginPopupModal("Movement Test", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("This will test movement to each node without vision detection.");
    ImGui::Text("Nodes: %s, %s, %s", m_node1Name, m_node2Name, m_node3Name);

    if (ImGui::Button("Start Test", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

// =============================================================================
// DIALOG RENDERING
// =============================================================================

void ModuleAlignmentUI::RenderSaveDialog() {
  if (!m_showSaveDialog) return;

  ImGui::OpenPopup("Save Alignment");

  if (ImGui::BeginPopupModal("Save Alignment", &m_showSaveDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Save current alignment configuration:");
    ImGui::Spacing();

    ImGui::InputText("Alignment Name", m_saveAlignmentName, sizeof(m_saveAlignmentName));

    ImGui::Spacing();

    bool canSave = strlen(m_saveAlignmentName) > 0 && m_hasResult && m_lastResult.success;

    if (!canSave) {
      ImGui::BeginDisabled();
    }

    if (ImGui::Button("Save", ImVec2(120, 0))) {
      if (m_moduleAlignment && m_moduleAlignment->SaveAlignmentData(m_saveAlignmentName)) {
        RefreshSavedAlignments();
        m_showSaveDialog = false;
      }
    }

    if (!canSave) {
      ImGui::EndDisabled();
      if (!m_hasResult || !m_lastResult.success) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "No successful alignment to save");
      }
      else if (strlen(m_saveAlignmentName) == 0) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Please enter a name");
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      m_showSaveDialog = false;
    }

    ImGui::EndPopup();
  }
}

void ModuleAlignmentUI::RenderLoadDialog() {
  if (!m_showLoadDialog) return;

  ImGui::OpenPopup("Load Alignment");

  if (ImGui::BeginPopupModal("Load Alignment", &m_showLoadDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (m_selectedAlignment >= 0 && m_selectedAlignment < static_cast<int>(m_savedAlignments.size())) {
      std::string selectedName = m_savedAlignments[m_selectedAlignment];
      ImGui::Text("Load alignment: %s", selectedName.c_str());
      ImGui::Text("This will replace the current alignment data.");

      ImGui::Spacing();

      if (ImGui::Button("Load", ImVec2(120, 0))) {
        if (m_moduleAlignment && m_moduleAlignment->LoadAlignmentData(selectedName)) {
          m_lastResult = m_moduleAlignment->GetAlignmentResult();
          m_hasResult = true;
          m_currentState = AlignmentState::COMPLETED;
          m_showLoadDialog = false;
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        m_showLoadDialog = false;
      }
    }
    else {
      ImGui::Text("No alignment selected.");
      if (ImGui::Button("OK", ImVec2(120, 0))) {
        m_showLoadDialog = false;
      }
    }

    ImGui::EndPopup();
  }
}

void ModuleAlignmentUI::RenderDeleteConfirmDialog() {
  if (!m_showDeleteDialog) return;

  ImGui::OpenPopup("Delete Alignment");

  if (ImGui::BeginPopupModal("Delete Alignment", &m_showDeleteDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (m_selectedAlignment >= 0 && m_selectedAlignment < static_cast<int>(m_savedAlignments.size())) {
      std::string selectedName = m_savedAlignments[m_selectedAlignment];
      ImGui::Text("Delete alignment: %s", selectedName.c_str());
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "This action cannot be undone!");

      ImGui::Spacing();

      if (ImGui::Button("Delete", ImVec2(120, 0))) {
        if (m_moduleAlignment && m_moduleAlignment->DeleteAlignmentData(selectedName)) {
          RefreshSavedAlignments();
          m_selectedAlignment = -1;
          m_showDeleteDialog = false;
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        m_showDeleteDialog = false;
      }
    }
    else {
      ImGui::Text("No alignment selected.");
      if (ImGui::Button("OK", ImVec2(120, 0))) {
        m_showDeleteDialog = false;
      }
    }

    ImGui::EndPopup();
  }
}

// =============================================================================
// UTILITY METHODS
// =============================================================================

void ModuleAlignmentUI::StartAlignment() {
  if (!m_moduleAlignment || !m_moduleAlignment->IsReadyForAlignment()) {
    return;
  }

  // Validate node names
  if (strlen(m_node1Name) == 0 || strlen(m_node2Name) == 0 || strlen(m_node3Name) == 0) {
    return;
  }

  // Check for duplicate node names
  if (strcmp(m_node1Name, m_node2Name) == 0 ||
    strcmp(m_node1Name, m_node3Name) == 0 ||
    strcmp(m_node2Name, m_node3Name) == 0) {
    m_statusMessage = "Error: All three node names must be different";
    m_currentState = AlignmentState::FAILED;
    return;
  }

  // Reset state
  m_currentState = AlignmentState::RUNNING;
  m_progressValue = 0.0f;
  m_currentStep = "Initializing alignment...";
  m_statusMessage = "Starting 3-point alignment process";
  m_hasResult = false;

  m_alignmentStartTime = std::chrono::steady_clock::now();

  std::cout << "ModuleAlignmentUI: Starting alignment with nodes: "
    << m_node1Name << ", " << m_node2Name << ", " << m_node3Name << std::endl;

  // Start alignment in background thread
  m_alignmentFuture = std::async(std::launch::async, [this]() -> ModuleAlignment::AlignmentResult {
    return m_moduleAlignment->PerformThreePointAlignment(
      m_node1Name, m_node2Name, m_node3Name, m_useRobotZ);
  });
}

void ModuleAlignmentUI::UpdateProgress() {
  if (m_currentState != AlignmentState::RUNNING || !m_alignmentFuture.valid()) {
    return;
  }

  // Check if alignment is complete
  if (m_alignmentFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
    // Get results
    try {
      m_lastResult = m_alignmentFuture.get();
      m_hasResult = true;

      // Update state
      if (m_lastResult.success) {
        m_currentState = AlignmentState::COMPLETED;
        m_progressValue = 1.0f;
        m_currentStep = "Alignment completed successfully";
        m_statusMessage = "✓ 3-point alignment finished successfully!";

        if (m_autoSwitchToResults) {
          m_activeTab = 1; // Switch to Results tab
        }

        std::cout << "ModuleAlignmentUI: Alignment completed successfully" << std::endl;
      }
      else {
        m_currentState = AlignmentState::FAILED;
        m_progressValue = 0.0f;
        m_currentStep = "Alignment failed";
        m_statusMessage = "❌ " + m_lastResult.errorMessage;

        if (m_autoSwitchToResults) {
          m_activeTab = 1;
        }

        std::cout << "ModuleAlignmentUI: Alignment failed: " << m_lastResult.errorMessage << std::endl;
      }
    }
    catch (const std::exception& e) {
      m_currentState = AlignmentState::FAILED;
      m_progressValue = 0.0f;
      m_currentStep = "Alignment failed with exception";
      m_statusMessage = "❌ Exception: " + std::string(e.what());
      m_hasResult = false;

      std::cout << "ModuleAlignmentUI: Alignment failed with exception: " << e.what() << std::endl;
    }

    // Reset the future
    m_alignmentFuture = std::future<ModuleAlignment::AlignmentResult>();
  }
  else {
    // Update progress simulation
    auto elapsed = std::chrono::steady_clock::now() - m_alignmentStartTime;
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    m_progressValue = (std::min)(0.95f, elapsedMs / 30000.0f);

    // Update step messages
    if (elapsedMs < 5000) {
      m_currentStep = "🎯 Moving to " + std::string(m_node1Name) + "...";
    }
    else if (elapsedMs < 15000) {
      m_currentStep = "🎯 Moving to " + std::string(m_node2Name) + "...";
    }
    else if (elapsedMs < 25000) {
      m_currentStep = "🎯 Moving to " + std::string(m_node3Name) + "...";
    }
    else {
      m_currentStep = "🧮 Calculating coordinate system...";
    }

    m_statusMessage = "⏳ Alignment in progress. Please wait...";
  }
}

void ModuleAlignmentUI::RefreshSavedAlignments() {
  m_savedAlignments.clear();
  m_selectedAlignment = -1;

  if (m_moduleAlignment) {
    m_savedAlignments = m_moduleAlignment->GetSavedAlignments();
  }
}

void ModuleAlignmentUI::ResetAlignment() {
  m_currentState = AlignmentState::IDLE;
  m_progressValue = 0.0f;
  m_currentStep = "";
  m_statusMessage = "";
}

bool ModuleAlignmentUI::IsAlignmentRunning() const {
  bool stateRunning = (m_currentState == AlignmentState::RUNNING);
  bool futureValid = m_alignmentFuture.valid();
  bool futureNotReady = futureValid &&
    (m_alignmentFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready);

  return stateRunning && futureValid && futureNotReady;
}

std::string ModuleAlignmentUI::FormatPosition(const PositionStruct& pos) const {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(3);
  oss << "(" << pos.x << ", " << pos.y << ", " << pos.z << ")";
  return oss.str();
}

std::string ModuleAlignmentUI::FormatVector(const ModuleAlignment::Vector3D& vec) const {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(4);
  oss << "(" << vec.x << ", " << vec.y << ", " << vec.z << ")";
  return oss.str();
}

ImVec4 ModuleAlignmentUI::GetStateColor(AlignmentState state) const {
  switch (state) {
  case AlignmentState::IDLE:      return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
  case AlignmentState::RUNNING:   return ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
  case AlignmentState::COMPLETED: return ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
  case AlignmentState::FAILED:    return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
  default:                        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
  }
}

const char* ModuleAlignmentUI::GetStateText(AlignmentState state) const {
  switch (state) {
  case AlignmentState::IDLE:      return "Idle";
  case AlignmentState::RUNNING:   return "Running";
  case AlignmentState::COMPLETED: return "Completed";
  case AlignmentState::FAILED:    return "Failed";
  default:                        return "Unknown";
  }
}

double ModuleAlignmentUI::CalculateAngleBetweenVectors(const ModuleAlignment::Vector3D& v1,
  const ModuleAlignment::Vector3D& v2) const {
  double dot = v1.dot(v2);
  double mag1 = v1.magnitude();
  double mag2 = v2.magnitude();

  if (mag1 < 1e-10 || mag2 < 1e-10) {
    return 0.0;
  }

  double cosAngle = dot / (mag1 * mag2);
  cosAngle = (std::max)(-1.0, (std::min)(1.0, cosAngle));

  return std::acos(cosAngle) * 180.0 / M_PI;
}

void ModuleAlignmentUI::ExecuteMoveToLocal() {
  if (!m_moduleAlignment || !m_moduleAlignment->HasValidAlignment()) {
    m_lastMoveStatus = "No valid alignment available";
    m_lastMoveSuccess = false;
    return;
  }

  if (!m_moduleAlignment->IsReadyForAlignment()) {
    m_lastMoveStatus = "System not ready for movement";
    m_lastMoveSuccess = false;
    return;
  }

  // Execute the move
  bool success = m_moduleAlignment->MoveToLocalCoordinate(
    m_targetLocalPos[0],
    m_targetLocalPos[1],
    m_targetLocalPos[2],
    m_deviceName,
    m_waitForCompletion
  );

  m_lastMoveSuccess = success;

  if (success) {
    m_lastMoveStatus = "Move to (" +
      std::to_string(m_targetLocalPos[0]) + ", " +
      std::to_string(m_targetLocalPos[1]) + ", " +
      std::to_string(m_targetLocalPos[2]) + ") completed";

    GetCurrentLocalPosition();

    std::cout << "ModuleAlignmentUI: Successfully moved to local position ("
      << m_targetLocalPos[0] << ", " << m_targetLocalPos[1] << ", " << m_targetLocalPos[2] << ")" << std::endl;
  }
  else {
    m_lastMoveStatus = "Move failed: " + m_moduleAlignment->GetLastError();
    std::cout << "ModuleAlignmentUI: Move to local position failed: " << m_moduleAlignment->GetLastError() << std::endl;
  }
}

void ModuleAlignmentUI::GetCurrentLocalPosition() {
  if (!m_moduleAlignment || !m_moduleAlignment->HasValidAlignment()) {
    m_currentLocalPos[0] = 0.0f;
    m_currentLocalPos[1] = 0.0f;
    m_currentLocalPos[2] = 0.0f;
    return;
  }

  PositionStruct localPos;
  bool success = m_moduleAlignment->GetCurrentLocalPosition(m_deviceName, localPos);

  if (success) {
    m_currentLocalPos[0] = static_cast<float>(localPos.x);
    m_currentLocalPos[1] = static_cast<float>(localPos.y);
    m_currentLocalPos[2] = static_cast<float>(localPos.z);

    std::cout << "ModuleAlignmentUI: Current local position: ("
      << localPos.x << ", " << localPos.y << ", " << localPos.z << ")" << std::endl;
  }
  else {
    std::cout << "ModuleAlignmentUI: Failed to get current local position: " << m_moduleAlignment->GetLastError() << std::endl;
  }
}