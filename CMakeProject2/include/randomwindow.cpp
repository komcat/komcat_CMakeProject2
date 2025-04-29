#include "include/randomwindow.h"
#include "include/random.h"
#include "include/logger.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include <SDL_opengl.h>
#include <sstream>

RandomWindow::RandomWindow()
    : random_value(0.0f)
    , custom_range_value(0.0f)
    , min_range(-10.0f)
    , max_range(10.0f)
{
    // Initialize random array
    for (int i = 0; i < 5; i++) {
        random_array[i] = 0.0f;
    }

    // Log initialization
    Logger::GetInstance()->LogInfo("RandomWindow: Initialized with default values");
}

void RandomWindow::Render() {
    // Create ImGui window
    ImGui::Begin("Random Number Generator");

    // Basic random value (0-1)
    if (ImGui::Button("Generate Random Number (0-1)")) {
        random_value = randomf();

        // Log the generated value with Info level
        std::stringstream ss;
        ss << "RandomWindow: Generated random value (0-1): " << random_value;
        Logger::GetInstance()->LogInfo(ss.str());
    }
    ImGui::Text("Random Value: %.6f", random_value);

    ImGui::Separator();

    // Custom range random value
    ImGui::SliderFloat("Min Value", &min_range, -100.0f, 100.0f);
    ImGui::SliderFloat("Max Value", &max_range, -100.0f, 100.0f);

    if (ImGui::Button("Generate Random Number (Custom Range)")) {
        

        // Log with Warning level if min > max
        if (min_range > max_range) {
            Logger::GetInstance()->LogWarning("RandomWindow: Min value is greater than Max value, swapping them");
        }
        else
        {
            custom_range_value = randomf(min_range, max_range);
            // Log the generated value with Info level
            std::stringstream ss;
            ss << "RandomWindow: Generated random value (" << min_range << " to " << max_range << "): " << custom_range_value;
            Logger::GetInstance()->LogInfo(ss.str());
        }

    }
    ImGui::Text("Custom Range Value: %.6f", custom_range_value);

    ImGui::Separator();

    // Multiple random values
    if (ImGui::Button("Generate 5 Random Numbers")) {
        Logger::GetInstance()->LogInfo("RandomWindow: Generating 5 random values");

        for (int i = 0; i < 5; i++) {
            random_array[i] = randomf();

            // Log each generated value - use Error level for the 3rd one as example
            std::stringstream ss;
            ss << "RandomWindow: Value " << (i + 1) << ": " << random_array[i];

            Logger::GetInstance()->LogInfo(ss.str());
        }
    }

    for (int i = 0; i < 5; i++) {
        ImGui::Text("Value %d: %.6f", i + 1, random_array[i]);
    }

    ImGui::End();
}

bool RandomWindow::IsDone() const {
    // This will be called from main to check if window should close
    // For now, we always return false since we handle window closing in main
    return false;
}