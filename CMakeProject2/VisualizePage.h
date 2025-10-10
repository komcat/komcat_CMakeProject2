// VisualizePage.h - Graph visualization from motion_config.json
#pragma once

#include <vector>
#include <map>
#include <string>

// Forward declaration only - no raylib includes in header
class Logger;

class VisualizePage {
public:
  VisualizePage(Logger* logger);
  ~VisualizePage();

  // Simple interface
  void Render();

private:
  Logger* m_logger;

  // Graph data structures
  struct Node {
    std::string id;
    std::string label;
    std::string device;
    float x;  // World coordinates from JSON
    float y;
  };

  struct Edge {
    std::string id;
    std::string source;
    std::string target;
    std::string label;
    bool isBidirectional;
  };

  std::vector<Node> m_nodes;
  std::vector<Edge> m_edges;
  std::map<std::string, Node*> m_nodeMap;  // For quick lookup by ID

  // View state (camera/viewport)
  float m_viewCenterX = 0.0f;  // Center of view in world space
  float m_viewCenterY = 0.0f;
  float m_viewScale = 1.0f;    // Pixels per world unit (zoom)

  // Interaction state
  struct {
    float x, y;
  } m_dragStart;
  bool m_isDragging = false;

  // Filtering
  std::string m_activeDeviceFilter = "";  // Empty = show all
  std::vector<std::string> m_availableDevices;

  // Font support (keep as void* to avoid raylib includes in header)
  void* m_customFont;  // Will be cast to Font in .cpp
  bool m_fontLoaded;

  // Graph loading and setup
  void loadGraphFromJSON();
  void calculateInitialView();
  void extractAvailableDevices();

  // Coordinate transforms
  struct Vector2Float {
    float x, y;
  };
  Vector2Float worldToScreen(float worldX, float worldY);
  Vector2Float screenToWorld(float screenX, float screenY);

  // Interaction handling
  void handleZoom();
  void handlePan();
  void handleResetView();

  // Rendering
  void renderFilterButtons();
  void renderGraph();
  void renderEdge(const Edge& edge);
  void renderNode(const Node& node);
  void renderUI();

  // Filtering helpers
  bool isNodeVisible(const Node& node);
  bool isEdgeVisible(const Edge& edge);

  // Device color mapping
// In the private section, use a simple struct or just return values directly
  struct DeviceColor { unsigned char r, g, b, a; };
  DeviceColor getDeviceColor(const std::string& device);
};