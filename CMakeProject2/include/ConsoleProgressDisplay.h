// ConsoleProgressDisplay.h
#pragma once

#include "ApplicationInitializer.h"
#include <iostream>
#include <iomanip>

class ConsoleProgressDisplay {
public:
  static void DisplayProgress(const ApplicationInitializer::InitProgress& progress) {
    // Clear line and print progress
    std::cout << "\r" << std::string(100, ' ') << "\r";

    // Progress bar
    int barWidth = 50;
    int pos = static_cast<int>(barWidth * progress.percentage / 100.0f);

    std::cout << "[";
    for (int i = 0; i < barWidth; ++i) {
      if (i < pos) std::cout << "=";
      else if (i == pos) std::cout << ">";
      else std::cout << " ";
    }
    std::cout << "] ";

    // Percentage
    std::cout << std::setw(3) << (int)progress.percentage << "% ";

    // Current operation
    std::cout << progress.currentOperation;

    if (progress.hasError) {
      std::cout << " ERROR: " << progress.errorMessage;
    }

    std::cout << std::flush;
  }

  static void Complete() {
    std::cout << "\n=== Initialization Complete ===\n";
  }
};