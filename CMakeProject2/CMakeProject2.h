// CMakeProject2.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>

// TODO: Reference additional headers your program requires here.
#include <math.h>

// Forward declare SDL_Window struct
struct SDL_Window;

bool enableDebug = false;
// Add this before the main function
void RenderValueDisplay();
void RenderSimpleChart();
void InitializeImPlot();
void ShutdownImPlot();

void RenderDraggableOverlay();
// Corner parameter: 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right
void RenderClockOverlay(int corner = 1);
void RenderGlobalDataValue(const std::string& dataName);
void RenderGlobalDataValueSI(const std::string& dataName);
void RenderMinimizeExitButtons(SDL_Window* window, bool& done);
void RenderFPSoverlay(float m_fps);