// toolbar.h
#pragma once

#include "include/motions/MotionConfigEditor.h"
#include "include/ui/GraphVisualizer.h"
#include "include/eziio/EziIO_UI.h"  // Add this include
#include "include/logger.h"
#include <functional>

class Toolbar {
public:
  Toolbar(MotionConfigEditor& configEditor, GraphVisualizer& graphVisualizer, EziIO_UI& ioUI);  // Updated constructor
  ~Toolbar() = default;

  // Render the toolbar UI
  void RenderUI();

  // Set callbacks for custom buttons
  void SetButton2Callback(std::function<void()> callback);
  void SetButton3Callback(std::function<void()> callback);

private:
  // Reference to the Motion Config Editor, Graph Visualizer, and EziIO_UI
  MotionConfigEditor& m_configEditor;
  GraphVisualizer& m_graphVisualizer;
  EziIO_UI& m_ioUI;  // Add this reference

  // Callbacks for custom buttons
  std::function<void()> m_button2Callback;
  std::function<void()> m_button3Callback;

  // Button states
  bool m_configEditorVisible;
  bool m_graphVisualizerVisible;
  bool m_button2Active = false;
  bool m_button3Active = false;
};