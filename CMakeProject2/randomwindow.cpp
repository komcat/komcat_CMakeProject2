#include "randomwindow.h"
#include "random.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include <SDL_opengl.h>

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
}

void RandomWindow::Render() {
    // Create ImGui window
    ImGui::Begin("Random Number Generator");

    // Basic random value (0-1)
    if (ImGui::Button("Generate Random Number (0-1)")) {
        random_value = randomf();
    }
    ImGui::Text("Random Value: %.6f", random_value);

    ImGui::Separator();


    // Custom range random value
    ImGui::SliderFloat("Min Value", &min_range, -100.0f, 100.0f);
    ImGui::SliderFloat("Max Value", &max_range, -100.0f, 100.0f);

    if (ImGui::Button("Generate Random Number (Custom Range)")) {
        custom_range_value = randomf(min_range, max_range);
    }
    ImGui::Text("Custom Range Value: %.6f", custom_range_value);

    ImGui::Separator();

    // Multiple random values
    if (ImGui::Button("Generate 5 Random Numbers")) {
        for (int i = 0; i < 5; i++) {
            random_array[i] = randomf();
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