// VisualizePage.cpp - Graph visualization implementation
#include "VisualizePage.h"
#include "include/logger.h"

// ONLY include raylib in the .cpp file
#include <raylib.h>
#include <math.h>
#include <set>           // ← ADD THIS LINE
#include <fstream>
#include <algorithm>

// For JSON parsing - assumes nlohmann/json is available
// If not available, you'll need to add it or use another JSON library
#include <nlohmann/json.hpp>
using json = nlohmann::json;

VisualizePage::VisualizePage(Logger* logger)
  : m_logger(logger), m_customFont(nullptr), m_fontLoaded(false),
  m_viewCenterX(0.0f), m_viewCenterY(0.0f), m_viewScale(1.0f),
  m_isDragging(false) {

  if (m_logger) {
    m_logger->LogInfo("VisualizePage created - Graph mode");
  }

  // Load the custom font
  Font* customFont = new Font();
  *customFont = LoadFont("assets/fonts/CascadiaCode-Regular.ttf");

  if (customFont->texture.id != 0) {
    m_customFont = customFont;
    m_fontLoaded = true;
    if (m_logger) {
      m_logger->LogInfo("CascadiaCode-Regular font loaded successfully");
    }
  }
  else {
    delete customFont;
    m_customFont = nullptr;
    m_fontLoaded = false;
    if (m_logger) {
      m_logger->LogWarning("Failed to load CascadiaCode-Regular font, using default");
    }
  }

  // Load graph data from JSON
  loadGraphFromJSON();

  // Setup initial view to fit all nodes
  if (!m_nodes.empty()) {
    calculateInitialView();
  }

  // Extract available devices for filtering
  extractAvailableDevices();
}

VisualizePage::~VisualizePage() {
  // Clean up font
  if (m_fontLoaded && m_customFont) {
    Font* customFont = static_cast<Font*>(m_customFont);
    UnloadFont(*customFont);
    delete customFont;
    m_customFont = nullptr;
  }

  if (m_logger) {
    m_logger->LogInfo("VisualizePage destroyed");
  }
}

void VisualizePage::loadGraphFromJSON() {
  try {
    std::ifstream file("motion_config.json");
    if (!file.is_open()) {
      if (m_logger) {
        m_logger->LogError("Failed to open motion_config.json");
      }
      return;
    }

    json config = json::parse(file);
    file.close();

    if (!config.contains("Graphs") || !config["Graphs"].contains("Process_Flow")) {
      if (m_logger) {
        m_logger->LogError("motion_config.json missing Graphs.Process_Flow");
      }
      return;
    }

    json processFlow = config["Graphs"]["Process_Flow"];

    // ✅ RESERVE SPACE to prevent reallocation
    if (processFlow.contains("Nodes")) {
      m_nodes.reserve(processFlow["Nodes"].size());
    }

    // Load nodes
    if (processFlow.contains("Nodes")) {
      for (const auto& jsonNode : processFlow["Nodes"]) {
        Node node;
        node.id = jsonNode.value("Id", "");
        node.label = jsonNode.value("Label", "");
        node.device = jsonNode.value("Device", "");
        node.x = jsonNode.value("X", 0.0f);
        node.y = jsonNode.value("Y", 0.0f);

        m_nodes.push_back(node);
        m_nodeMap[node.id] = &m_nodes.back();  // Now safe - no reallocation
      }
    }

    // Load edges
    if (processFlow.contains("Edges")) {
      for (const auto& jsonEdge : processFlow["Edges"]) {
        Edge edge;
        edge.id = jsonEdge.value("Id", "");
        edge.source = jsonEdge.value("Source", "");
        edge.target = jsonEdge.value("Target", "");
        edge.label = jsonEdge.value("Label", "");

        if (jsonEdge.contains("Conditions")) {
          edge.isBidirectional = jsonEdge["Conditions"].value("IsBidirectional", false);
        }
        else {
          edge.isBidirectional = false;
        }

        m_edges.push_back(edge);
      }
    }

    if (m_logger) {
      m_logger->LogInfo("Loaded graph: " + std::to_string(m_nodes.size()) +
        " nodes, " + std::to_string(m_edges.size()) + " edges");
    }

  }
  catch (const std::exception& e) {
    if (m_logger) {
      m_logger->LogError("Error loading graph: " + std::string(e.what()));
    }
  }
}


void VisualizePage::calculateInitialView() {
  if (m_nodes.empty()) return;

  // Find bounding box - ONLY for visible nodes
  bool foundAny = false;
  float minX = 0, maxX = 0, minY = 0, maxY = 0;

  for (const auto& node : m_nodes) {
    // SKIP nodes that don't match the active filter
    if (!isNodeVisible(node)) continue;

    if (!foundAny) {
      minX = node.x;
      maxX = node.x;
      minY = node.y;
      maxY = node.y;
      foundAny = true;
    }
    else {
      minX = std::min(minX, node.x);
      maxX = std::max(maxX, node.x);
      minY = std::min(minY, node.y);
      maxY = std::max(maxY, node.y);
    }
  }

  if (!foundAny) return; // No visible nodes

  // Set view center to center of visible graph
  m_viewCenterX = (minX + maxX) / 2.0f;
  m_viewCenterY = (minY + maxY) / 2.0f;

  // Calculate scale to fit with margin
  float worldWidth = maxX - minX;
  float worldHeight = maxY - minY;

  if (worldWidth < 1.0f) worldWidth = 100.0f;  // Minimum width
  if (worldHeight < 1.0f) worldHeight = 100.0f; // Minimum height

  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  int graphArea = screenHeight - 120;  // Leave space for UI

  float margin = 0.85f;  // Use 85% of available space
  float scaleX = (screenWidth * margin) / worldWidth;
  float scaleY = (graphArea * margin) / worldHeight;

  m_viewScale = std::min(scaleX, scaleY);

  if (m_logger) {
    m_logger->LogInfo("View recalculated: center(" + std::to_string(m_viewCenterX) +
      ", " + std::to_string(m_viewCenterY) +
      ") scale=" + std::to_string(m_viewScale));
  }
}



void VisualizePage::extractAvailableDevices() {
  std::set<std::string> deviceSet;
  for (const auto& node : m_nodes) {
    if (!node.device.empty()) {
      deviceSet.insert(node.device);
    }
  }
  m_availableDevices = std::vector<std::string>(deviceSet.begin(), deviceSet.end());
}

VisualizePage::Vector2Float VisualizePage::worldToScreen(float worldX, float worldY) {
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();

  return {
    (worldX - m_viewCenterX) * m_viewScale + screenWidth / 2.0f,
    (worldY - m_viewCenterY) * m_viewScale + screenHeight / 2.0f
  };
}

VisualizePage::Vector2Float VisualizePage::screenToWorld(float screenX, float screenY) {
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();

  return {
    (screenX - screenWidth / 2.0f) / m_viewScale + m_viewCenterX,
    (screenY - screenHeight / 2.0f) / m_viewScale + m_viewCenterY
  };
}

void VisualizePage::handleZoom() {
  float wheel = GetMouseWheelMove();
  if (wheel != 0) {
    Vector2 mousePos = GetMousePosition();

    // Get world position under mouse BEFORE zoom
    auto worldBefore = screenToWorld(mousePos.x, mousePos.y);

    // Apply zoom
    float zoomFactor = wheel > 0 ? 1.15f : (1.0f / 1.15f);
    m_viewScale *= zoomFactor;

    // Clamp zoom range
    m_viewScale = std::max(0.01f, std::min(20.0f, m_viewScale));

    // Get world position under mouse AFTER zoom
    auto worldAfter = screenToWorld(mousePos.x, mousePos.y);

    // Adjust center to keep mouse position fixed
    m_viewCenterX += worldBefore.x - worldAfter.x;
    m_viewCenterY += worldBefore.y - worldAfter.y;
  }
}

void VisualizePage::handlePan() {
  Vector2 mousePos = GetMousePosition();

  // Start drag
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mousePos.y > 100) {
    m_dragStart.x = mousePos.x;
    m_dragStart.y = mousePos.y;
    m_isDragging = true;
  }

  // Continue drag
  if (m_isDragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    float deltaX = mousePos.x - m_dragStart.x;
    float deltaY = mousePos.y - m_dragStart.y;

    m_viewCenterX -= deltaX / m_viewScale;
    m_viewCenterY -= deltaY / m_viewScale;

    m_dragStart.x = mousePos.x;
    m_dragStart.y = mousePos.y;
  }

  // End drag
  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    m_isDragging = false;
  }
}

void VisualizePage::handleResetView() {
  if (IsKeyPressed(KEY_F)) {
    calculateInitialView();  // This now respects the filter
    if (m_logger) {
      m_logger->LogInfo("View reset to fit visible nodes");
    }
  }
}



bool VisualizePage::isNodeVisible(const Node& node) {
  if (m_activeDeviceFilter.empty()) return true;
  return node.device == m_activeDeviceFilter;
}

bool VisualizePage::isEdgeVisible(const Edge& edge) {
  // Edge is visible if both nodes exist AND are visible
  auto sourceIt = m_nodeMap.find(edge.source);
  auto targetIt = m_nodeMap.find(edge.target);

  if (sourceIt == m_nodeMap.end() || targetIt == m_nodeMap.end()) {
    return false;  // One or both nodes don't exist
  }

  const Node* sourceNode = sourceIt->second;
  const Node* targetNode = targetIt->second;

  // CRITICAL: Both nodes must be visible
  if (!isNodeVisible(*sourceNode) || !isNodeVisible(*targetNode)) {
    return false;
  }

  // OPTIONAL: Extra check - both nodes must be same device
  // Uncomment if you want to enforce same-device edges only
  // if (sourceNode->device != targetNode->device) {
  //   return false;
  // }

  return true;
}

// Change the return type
VisualizePage::DeviceColor VisualizePage::getDeviceColor(const std::string& device) {
  // Color mapping for different devices
  if (device == "gantry-main") return { 100, 200, 255, 255 };  // Light blue
  if (device == "hex-left") return { 255, 150, 100, 255 };     // Orange
  if (device == "hex-right") return { 150, 255, 150, 255 };    // Light green
  if (device == "xy-table") return { 255, 200, 100, 255 };     // Yellow
  if (device == "hex-bottom") return { 200, 150, 255, 255 };   // Purple
  return { 180, 180, 180, 255 };  // Default gray
}

void VisualizePage::renderFilterButtons() {
  Font font = m_fontLoaded ? *static_cast<Font*>(m_customFont) : GetFontDefault();

  int buttonY = 45;
  int buttonHeight = 30;
  int buttonSpacing = 10;
  int currentX = 10;

  Vector2 mousePos = GetMousePosition();

  // "All" button
  std::vector<std::string> filters = { "All" };
  filters.insert(filters.end(), m_availableDevices.begin(), m_availableDevices.end());

  for (size_t i = 0; i < filters.size(); ++i) {
    const std::string& filter = filters[i];

    // Measure button width based on text
    const char* filterText = filter.c_str();
    Vector2 textSize = MeasureTextEx(font, filterText, 14, 1);
    int buttonWidth = (int)textSize.x + 20;

    Rectangle buttonRect = { (float)currentX, (float)buttonY, (float)buttonWidth, (float)buttonHeight };

    bool isActive = (i == 0 && m_activeDeviceFilter.empty()) ||
      (i > 0 && m_activeDeviceFilter == filter);
    bool isHovered = CheckCollisionPointRec(mousePos, buttonRect);

    // Button colors
// Button colors
    Color bgColor;  // This is raylib's Color, no :: needed
    if (isActive) {
      bgColor = { 80, 120, 160, 255 };
    }
    else if (isHovered) {
      bgColor = { 60, 60, 70, 255 };
    }
    else {
      bgColor = { 45, 45, 55, 255 };
    }

    Color borderColor = isActive ? Color{ 100, 150, 200, 255 } : DARKGRAY;  // No :: needed
    Color textColor = isActive ? WHITE : LIGHTGRAY;  // No :: needed

    DrawRectangleRec(buttonRect, bgColor);
    DrawRectangleLinesEx(buttonRect, isActive ? 2.0f : 1.0f, borderColor);

    Vector2 textPos = {
      buttonRect.x + buttonRect.width / 2 - textSize.x / 2,
      buttonRect.y + buttonRect.height / 2 - textSize.y / 2
    };
    DrawTextEx(font, filterText, textPos, 14, 1, textColor);

    // Handle click
    if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      if (i == 0) {
        m_activeDeviceFilter = "";
      }
      else {
        m_activeDeviceFilter = filter;
      }

      // RECALCULATE VIEW to fit visible nodes
      calculateInitialView();  // ADD THIS LINE

      if (m_logger) {
        m_logger->LogInfo("Filter changed to: " + (m_activeDeviceFilter.empty() ? "All" : m_activeDeviceFilter));
      }
    }

    currentX += buttonWidth + buttonSpacing;
  }
}

void VisualizePage::renderEdge(const Edge& edge) {
  if (!isEdgeVisible(edge)) return;

  auto sourceIt = m_nodeMap.find(edge.source);
  auto targetIt = m_nodeMap.find(edge.target);

  if (sourceIt == m_nodeMap.end() || targetIt == m_nodeMap.end()) return;

  const Node* source = sourceIt->second;
  const Node* target = targetIt->second;

  auto startScreen = worldToScreen(source->x, source->y);
  auto endScreen = worldToScreen(target->x, target->y);

  // Simple culling - skip if both points far off screen
  int screenW = GetScreenWidth();
  int screenH = GetScreenHeight();
  bool startVisible = (startScreen.x >= -100 && startScreen.x <= screenW + 100 &&
    startScreen.y >= -100 && startScreen.y <= screenH + 100);
  bool endVisible = (endScreen.x >= -100 && endScreen.x <= screenW + 100 &&
    endScreen.y >= -100 && endScreen.y <= screenH + 100);

  if (!startVisible && !endVisible) return;

  // Calculate direction vector
  float dx = endScreen.x - startScreen.x;
  float dy = endScreen.y - startScreen.y;
  float length = sqrtf(dx * dx + dy * dy);

  if (length < 1e-6f) return; // Skip if nodes are at same position

  // Normalize direction
  dx /= length;
  dy /= length;

  // Node rectangle dimensions (scaled with zoom)
  float nodeRadius = std::max(4.0f, std::min(25.0f, 8.0f * m_viewScale));

  // Calculate edge start/end points at node boundaries (not centers)
  Vector2 sourceEdge = {
    startScreen.x + dx * nodeRadius,
    startScreen.y + dy * nodeRadius
  };

  Vector2 targetEdge = {
    endScreen.x - dx * nodeRadius,
    endScreen.y - dy * nodeRadius
  };

  // Draw edge line
  Color edgeColor = edge.isBidirectional ?
    Color{ 100, 200, 100, 200 } :   // Green for bidirectional
    Color{ 150, 150, 150, 200 };    // Gray for unidirectional

  float lineWidth = edge.isBidirectional ? 2.5f : 2.0f;

  DrawLineEx(sourceEdge, targetEdge, lineWidth, edgeColor);

  // Draw arrows
  if (m_viewScale > 0.3f) {
    float arrowSize = std::min(12.0f, 8.0f * m_viewScale);

    // End arrow (always drawn)
    Vector2 arrowP1 = {
      targetEdge.x - dx * arrowSize - dy * arrowSize * 0.5f,
      targetEdge.y - dy * arrowSize + dx * arrowSize * 0.5f
    };
    Vector2 arrowP2 = {
      targetEdge.x - dx * arrowSize + dy * arrowSize * 0.5f,
      targetEdge.y - dy * arrowSize - dx * arrowSize * 0.5f
    };

    DrawTriangle(targetEdge, arrowP1, arrowP2, edgeColor);

    // Start arrow (only for bidirectional)
    if (edge.isBidirectional) {
      Vector2 startArrowP1 = {
        sourceEdge.x + dx * arrowSize - dy * arrowSize * 0.5f,
        sourceEdge.y + dy * arrowSize + dx * arrowSize * 0.5f
      };
      Vector2 startArrowP2 = {
        sourceEdge.x + dx * arrowSize + dy * arrowSize * 0.5f,
        sourceEdge.y + dy * arrowSize - dx * arrowSize * 0.5f
      };

      DrawTriangle(sourceEdge, startArrowP1, startArrowP2, edgeColor);
    }
  }

  // Draw edge label if zoomed in enough
  if (m_viewScale > 0.6f && !edge.label.empty()) {
    float midX = (sourceEdge.x + targetEdge.x) / 2.0f;
    float midY = (sourceEdge.y + targetEdge.y) / 2.0f;

    Font font = m_fontLoaded ? *static_cast<Font*>(m_customFont) : GetFontDefault();
    int fontSize = (int)std::max(8.0f, std::min(12.0f, 10.0f * m_viewScale));
    Vector2 textSize = MeasureTextEx(font, edge.label.c_str(), (float)fontSize, 1);

    // Background for label
    DrawRectangle((int)(midX - textSize.x / 2 - 3), (int)(midY - textSize.y / 2 - 2),
      (int)textSize.x + 6, (int)textSize.y + 4, Color{ 40, 40, 40, 220 });

    DrawTextEx(font, edge.label.c_str(),
      Vector2{ midX - textSize.x / 2, midY - textSize.y / 2 },
      (float)fontSize, 1, Color{ 220, 220, 220, 255 });
  }
}


void VisualizePage::renderNode(const Node& node) {
  if (!isNodeVisible(node)) return;

  auto screenPos = worldToScreen(node.x, node.y);

  // Culling
  int screenW = GetScreenWidth();
  int screenH = GetScreenHeight();
  if (screenPos.x < -50 || screenPos.x > screenW + 50 ||
    screenPos.y < -50 || screenPos.y > screenH + 50) {
    return;
  }

  // ✅ INCREASED node size to match larger text
  float baseRadius = 12.0f;  // Increased from 8.0f
  float radius = std::max(6.0f, std::min(30.0f, baseRadius * m_viewScale));

  // Get device color
  auto deviceCol = getDeviceColor(node.device);
  Color nodeColor = { deviceCol.r, deviceCol.g, deviceCol.b, deviceCol.a };

  // Draw node circle
  DrawCircle((int)screenPos.x, (int)screenPos.y, radius, nodeColor);
  DrawCircleLines((int)screenPos.x, (int)screenPos.y, radius, BLACK);

  // ALWAYS DRAW ID AND LABEL
  Font font = m_fontLoaded ? *static_cast<Font*>(m_customFont) : GetFontDefault();

  // ✅ LARGE font size: min 28, max 48
  int fontSize = (int)std::max(28.0f, std::min(48.0f, 32.0f * m_viewScale));

  // Format: node_id on first line, <label> on second line
  std::string idText = node.id;
  std::string labelText = "<" + (node.label.empty() ? "" : node.label) + ">";

  // Measure text sizes
  Vector2 idTextSize = MeasureTextEx(font, idText.c_str(), (float)fontSize, 1);
  Vector2 labelTextSize = MeasureTextEx(font, labelText.c_str(), (float)fontSize, 1);

  // Calculate max width for background
  float maxWidth = std::max(idTextSize.x, labelTextSize.x);

  // Position: ID line first, label below
  Vector2 idTextPos = {
    screenPos.x - idTextSize.x / 2,
    screenPos.y + radius + 5  // ✅ Increased spacing from 3 to 5
  };

  Vector2 labelTextPos = {
    screenPos.x - labelTextSize.x / 2,
    idTextPos.y + idTextSize.y + 3  // ✅ Increased spacing from 2 to 3
  };

  // Background for both lines
  float bgPadding = 5.0f;  // ✅ Increased padding from 3 to 5
  float totalHeight = idTextSize.y + labelTextSize.y + 6;  // Increased spacing
  DrawRectangle(
    (int)(screenPos.x - maxWidth / 2 - bgPadding),
    (int)(idTextPos.y - 2),  // ✅ More top padding
    (int)(maxWidth + bgPadding * 2),
    (int)(totalHeight + 4),  // ✅ More bottom padding
    Color{ 0, 0, 0, 220 }
  );

  // Draw ID (brighter white)
  DrawTextEx(font, idText.c_str(), idTextPos, (float)fontSize, 1, WHITE);

  // Draw label (slightly dimmer)
  Color labelColor = { 220, 220, 220, 255 };
  DrawTextEx(font, labelText.c_str(), labelTextPos, (float)fontSize, 1, labelColor);
}

void VisualizePage::renderGraph() {
  // Draw edges first (underneath nodes)
  for (const auto& edge : m_edges) {
    renderEdge(edge);
  }

  // Draw nodes on top
  for (const auto& node : m_nodes) {
    renderNode(node);
  }
}

void VisualizePage::renderUI() {
  Font font = m_fontLoaded ? *static_cast<Font*>(m_customFont) : GetFontDefault();

  // Bottom info bar
  int infoY = GetScreenHeight() - 60;
  DrawRectangle(0, infoY, GetScreenWidth(), 60, Color{ 30, 30, 35, 255 });

  // Stats
  int visibleNodes = 0;
  int visibleEdges = 0;
  for (const auto& node : m_nodes) {
    if (isNodeVisible(node)) visibleNodes++;
  }
  for (const auto& edge : m_edges) {
    if (isEdgeVisible(edge)) visibleEdges++;
  }

  char statsText[256];
  snprintf(statsText, sizeof(statsText),
    "Nodes: %d/%d | Edges: %d/%d | Zoom: %.2fx",
    visibleNodes, (int)m_nodes.size(),
    visibleEdges, (int)m_edges.size(),
    m_viewScale);
  DrawTextEx(font, statsText, { 10, (float)infoY + 10 }, 14, 1, LIGHTGRAY);

  // Controls
  const char* controls = "Controls: Wheel=Zoom | Drag=Pan | F=Fit All | R=Visualize | M=Menu";
  DrawTextEx(font, controls, { 10, (float)infoY + 35 }, 12, 1, GRAY);

  // Current filter (right side)
  if (!m_activeDeviceFilter.empty()) {
    char filterText[64];
    snprintf(filterText, sizeof(filterText), "Filter: %s", m_activeDeviceFilter.c_str());
    DrawTextEx(font, filterText, { (float)GetScreenWidth() - 200, (float)infoY + 10 }, 14, 1, YELLOW);
  }

  // ✅ RELOAD BUTTON (Lower Left Corner)
  int buttonWidth = 120;
  int buttonHeight = 40;
  int buttonX = 10;
  int buttonY = GetScreenHeight() - 110;  // Above the info bar

  Rectangle reloadButton = { (float)buttonX, (float)buttonY, (float)buttonWidth, (float)buttonHeight };

  Vector2 mousePos = GetMousePosition();
  bool isHovered = CheckCollisionPointRec(mousePos, reloadButton);

  // Button colors
  Color buttonColor = isHovered ? Color{ 70, 140, 200, 255 } : Color{ 50, 100, 150, 255 };
  Color borderColor = isHovered ? Color{ 100, 180, 240, 255 } : Color{ 70, 120, 170, 255 };

  // Draw button
  DrawRectangleRec(reloadButton, buttonColor);
  DrawRectangleLinesEx(reloadButton, 2.0f, borderColor);

  // Button text
  const char* buttonText = "Reload JSON";
  int textFontSize = 14;
  Vector2 textSize = MeasureTextEx(font, buttonText, (float)textFontSize, 1);
  Vector2 textPos = {
    reloadButton.x + reloadButton.width / 2 - textSize.x / 2,
    reloadButton.y + reloadButton.height / 2 - textSize.y / 2
  };

  DrawTextEx(font, buttonText, textPos, (float)textFontSize, 1, WHITE);

  // Handle button click
  if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (m_logger) {
      m_logger->LogInfo("Reloading motion_config.json...");
    }

    // Clear existing data
    m_nodes.clear();
    m_edges.clear();
    m_nodeMap.clear();
    m_availableDevices.clear();

    // Reload from JSON
    loadGraphFromJSON();

    // Recalculate view
    if (!m_nodes.empty()) {
      calculateInitialView();
    }

    // Extract devices
    extractAvailableDevices();

    if (m_logger) {
      m_logger->LogInfo("JSON reloaded successfully! Nodes: " +
        std::to_string(m_nodes.size()) +
        ", Edges: " + std::to_string(m_edges.size()));
    }
  }

  // Show tooltip on hover
  if (isHovered) {
    const char* tooltip = "Click to reload graph from motion_config.json";
    Vector2 tooltipSize = MeasureTextEx(font, tooltip, 12, 1);
    Vector2 tooltipPos = {
      reloadButton.x + reloadButton.width / 2 - tooltipSize.x / 2,
      reloadButton.y - tooltipSize.y - 5
    };

    // Tooltip background
    DrawRectangle((int)tooltipPos.x - 4, (int)tooltipPos.y - 2,
      (int)tooltipSize.x + 8, (int)tooltipSize.y + 4,
      Color{ 0, 0, 0, 220 });

    DrawTextEx(font, tooltip, tooltipPos, 12, 1, Color{ 255, 255, 100, 255 });
  }
}


void VisualizePage::Render() {
  // Page title and navigation
  Font font = m_fontLoaded ? *static_cast<Font*>(m_customFont) : GetFontDefault();
  DrawTextEx(font, "Motion Graph Visualizer", { 10, 10 }, 20, 2, DARKBLUE);
  DrawTextEx(font, "R: Visualize | M: Menu | V: Live | S: Status | C: Chart", { 10, 35 }, 14, 1, GRAY);

  // Filter buttons
  renderFilterButtons();

  // Handle interactions
  handleZoom();
  handlePan();
  handleResetView();

  // Render the graph
  renderGraph();

  // Render UI overlay
  renderUI();
}