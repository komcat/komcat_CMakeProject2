// toolbar.h
#pragma once

#include "MotionConfigEditor.h"
#include "GraphVisualizer.h"
#include "logger.h"
#include <functional>

class Toolbar {
public:
    Toolbar(MotionConfigEditor& configEditor, GraphVisualizer& graphVisualizer);
    ~Toolbar() = default;

    // Render the toolbar UI
    void RenderUI();

    // Set callbacks for custom buttons
    void SetButton2Callback(std::function<void()> callback);
    void SetButton3Callback(std::function<void()> callback);

private:
    // Reference to the Motion Config Editor and Graph Visualizer
    MotionConfigEditor& m_configEditor;
    GraphVisualizer& m_graphVisualizer;

    // Callbacks for custom buttons
    std::function<void()> m_button2Callback;
    std::function<void()> m_button3Callback;


    // Button states
    bool m_configEditorVisible;
    bool m_graphVisualizerVisible;
    bool m_button2Active = false;
    bool m_button3Active = false;
};