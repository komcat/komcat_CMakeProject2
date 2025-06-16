// MacroEditState.h - Simple state machine for macro editing
#pragma once
#include <vector>
#include <string>

enum class ExecutionMode {
  SINGLE_PROGRAM,     // Run just selected program
  RUN_ALL,           // Run all programs sequentially  
  RUN_FROM_HERE,     // Run from selected program to end
  CUSTOM_SELECTION   // Run user-selected programs
};

class MacroEditState {
private:
  ExecutionMode m_mode = ExecutionMode::SINGLE_PROGRAM;
  std::vector<bool> m_selectedPrograms;
  int m_singleProgramIndex = -1;
  int m_runFromIndex = 0;

public:
  // Mode management
  void SetMode(ExecutionMode mode) { m_mode = mode; }
  ExecutionMode GetMode() const { return m_mode; }

  // Program selection
  void SetProgramCount(size_t count) {
    m_selectedPrograms.resize(count, false);
  }

  void SelectSingleProgram(int index) {
    m_singleProgramIndex = index;
    m_mode = ExecutionMode::SINGLE_PROGRAM;
  }

  void SetRunFromIndex(int index) {
    m_runFromIndex = index;
    m_mode = ExecutionMode::RUN_FROM_HERE;
  }

  void ToggleProgramSelection(int index) {
    if (index >= 0 && index < m_selectedPrograms.size()) {
      m_selectedPrograms[index] = !m_selectedPrograms[index];
      m_mode = ExecutionMode::CUSTOM_SELECTION;
    }
  }

  void SelectAllPrograms() {
    m_mode = ExecutionMode::RUN_ALL;
  }

  // Get execution list based on current state
  std::vector<int> GetExecutionIndices(size_t totalPrograms) const {
    std::vector<int> indices;

    switch (m_mode) {
    case ExecutionMode::SINGLE_PROGRAM:
      if (m_singleProgramIndex >= 0 && m_singleProgramIndex < totalPrograms) {
        indices.push_back(m_singleProgramIndex);
      }
      break;

    case ExecutionMode::RUN_ALL:
      for (int i = 0; i < totalPrograms; i++) {
        indices.push_back(i);
      }
      break;

    case ExecutionMode::RUN_FROM_HERE:
      for (int i = m_runFromIndex; i < totalPrograms; i++) {
        indices.push_back(i);
      }
      break;

    case ExecutionMode::CUSTOM_SELECTION:
      for (int i = 0; i < m_selectedPrograms.size() && i < totalPrograms; i++) {
        if (m_selectedPrograms[i]) {
          indices.push_back(i);
        }
      }
      break;
    }

    return indices;
  }

  // UI helpers
  bool IsProgramSelected(int index) const {
    if (m_mode == ExecutionMode::SINGLE_PROGRAM) {
      return index == m_singleProgramIndex;
    }
    if (m_mode == ExecutionMode::RUN_ALL) {
      return true;
    }
    if (m_mode == ExecutionMode::RUN_FROM_HERE) {
      return index >= m_runFromIndex;
    }
    if (m_mode == ExecutionMode::CUSTOM_SELECTION && index < m_selectedPrograms.size()) {
      return m_selectedPrograms[index];
    }
    return false;
  }

  std::string GetModeDescription() const {
    switch (m_mode) {
    case ExecutionMode::SINGLE_PROGRAM: return "Single Program";
    case ExecutionMode::RUN_ALL: return "Run All Sequential";
    case ExecutionMode::RUN_FROM_HERE: return "Run From Selected";
    case ExecutionMode::CUSTOM_SELECTION: return "Custom Selection";
    default: return "Unknown";
    }
  }
};
