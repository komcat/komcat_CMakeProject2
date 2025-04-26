
#pragma once

#include <SDL.h>
#include "imgui.h"

// Forward declarations
class RandomWindow {
private:
    // Variables for random number generation
    float random_value;
    float custom_range_value;
    float min_range;
    float max_range;
    float random_array[5];

public:
    // Constructor
    RandomWindow();

    // Render the ImGui window
    void Render();

    // Check if window should be closed
    bool IsDone() const;
};