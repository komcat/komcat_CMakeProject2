#include "ProcessControlPanel.h"
#include "imgui.h"
#include <chrono>
#include <iostream>

ProcessControlPanel::ProcessControlPanel(MachineOperations& machineOps)
	: m_machineOps(machineOps),
	m_uiManager(std::make_unique<MockUserInteractionManager>()),
	m_stopRequested(false)
{
	m_logger = Logger::GetInstance();
	m_logger->LogInfo("ProcessControlPanel: Initialized");
}

ProcessControlPanel::~ProcessControlPanel() {
	// Ensure any running process is stopped cleanly
	StopProcess();
	m_logger->LogInfo("ProcessControlPanel: Destroyed");
}

void ProcessControlPanel::RenderUI() {
	if (!m_showWindow) return;

	// Get display size for setting the initial window size
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 displaySize = io.DisplaySize;

	// Set initial window size to 60% width and 70% height (adjusted since we removed right panel)
	ImGui::SetNextWindowSize(ImVec2(displaySize.x * 0.6f, displaySize.y * 0.7f), ImGuiCond_FirstUseEver);

	// Begin window with resizable flag only - no auto resize
	ImGui::Begin("Process Control Panel", &m_showWindow);

	// Title with larger font
	ImGui::SetWindowFontScale(1.5f);
	ImGui::Text("Process Control");
	ImGui::SetWindowFontScale(1.0f);

	ImGui::Separator();

	// Single column layout for process buttons (now using full width)
	const float windowWidth = ImGui::GetContentRegionAvail().x;
	const float buttonWidth = windowWidth * 0.95f; // Use most of the available width
	const float buttonHeight = 45.0f; // Slightly taller buttons

	// Process buttons section
	ImGui::BeginChild("ProcessButtons", ImVec2(0, 400), true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
	ImGui::Text("Available Processes (Right-click for details):");
	ImGui::Separator();

	// Style for process selection buttons
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

	for (const auto& process : m_availableProcesses) {
		// Highlight the selected process with a green button
		if (m_selectedProcess == process) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.3f, 1.0f));
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
		}

		// Create a button for each process
		if (ImGui::Button(process.c_str(), ImVec2(buttonWidth, buttonHeight))) {
			m_selectedProcess = process;
			// If we're not already running a process, start this one right away
			if (!m_processRunning) {
				StartProcess(process);
			}
		}

		// Right-click context menu for process details
		if (ImGui::BeginPopupContextItem(("ProcessMenu_" + process).c_str())) {
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Process Details: %s", process.c_str());
			ImGui::Separator();

			// Get the process sequence and display its debug print
			try {
				// Temporarily store the current selected process
				std::string originalSelected = m_selectedProcess;
				m_selectedProcess = process;

				// Build the sequence to get the operations
				auto sequence = BuildSelectedProcess();
				if (sequence) {
					ImGui::Text("Operations in this process:");
					ImGui::Separator();

					const auto& operations = sequence->GetOperations();
					ImGui::Text("Total operations: %zu", operations.size());
					ImGui::Spacing();

					// Display each operation with step number
					for (size_t i = 0; i < operations.size(); ++i) {
						ImGui::Text("%zu. %s", i + 1, operations[i]->GetDescription().c_str());
					}

					ImGui::Spacing();
					ImGui::Separator();
					if (ImGui::Button("Close")) {
						ImGui::CloseCurrentPopup();
					}
				}
				else {
					ImGui::Text("Error: Could not build process sequence");
					if (ImGui::Button("Close")) {
						ImGui::CloseCurrentPopup();
					}
				}

				// Restore original selection
				m_selectedProcess = originalSelected;
			}
			catch (const std::exception& e) {
				ImGui::Text("Error building sequence: %s", e.what());
				if (ImGui::Button("Close")) {
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopStyleColor(2);

		// Spacing between buttons
		ImGui::Spacing();
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();

	ImGui::Separator();

	// Status section
	ImGui::Text("Status: %s", m_statusMessage.c_str());

	// Progress bar (if process is running)
	if (m_processRunning) {
		ImGui::ProgressBar(m_progress, ImVec2(-1, 0));
	}

	// Auto-confirm for user interactions
	bool autoConfirmValue = m_autoConfirm; // Copy to a local variable for ImGui
	if (ImGui::Checkbox("Auto-confirm User Interactions", &autoConfirmValue)) {
		// Update the class member when changed
		m_autoConfirm = autoConfirmValue;
		m_uiManager->SetAutoConfirm(m_autoConfirm);
	}

	// Start/Stop button at the bottom
	if (m_processRunning) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Red button
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));

		if (ImGui::Button("Stop Process", ImVec2(-1, 50))) {
			StopProcess();
		}

		ImGui::PopStyleColor(3);

		if (m_uiManager->IsWaitingForConfirmation() && !m_autoConfirm) {
			ImGui::Text("User confirmation needed: %s", m_uiManager->GetLastMessage().c_str());

			// CUSTOMIZABLE PARAMETERS:
			const float BUTTON_WIDTH = 150.0f;
			const float BUTTON_HEIGHT = 80.0f;
			const ImVec4 CONFIRM_COLOR = ImVec4(0.0f, 0.8f, 0.3f, 1.0f);
			const ImVec4 CONFIRM_HOVER = ImVec4(0.0f, 0.9f, 0.4f, 1.0f);
			const ImVec4 CANCEL_COLOR = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
			const ImVec4 CANCEL_HOVER = ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
			const float BUTTON_SPACING = 20.0f;

			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);

			// Confirm Button
			ImGui::PushStyleColor(ImGuiCol_Button, CONFIRM_COLOR);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CONFIRM_HOVER);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

			if (ImGui::Button("[Y] Confirm", ImVec2(BUTTON_WIDTH, BUTTON_HEIGHT))) {
				m_uiManager->ConfirmationReceived(true);
			}

			ImGui::PopStyleVar();
			ImGui::PopStyleColor(2);

			ImGui::SameLine(0, BUTTON_SPACING);

			// Cancel Button  
			ImGui::PushStyleColor(ImGuiCol_Button, CANCEL_COLOR);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CANCEL_HOVER);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

			if (ImGui::Button("[N] Cancel", ImVec2(BUTTON_WIDTH, BUTTON_HEIGHT))) {
				m_uiManager->ConfirmationReceived(false);
			}

			ImGui::PopStyleVar();
			ImGui::PopStyleColor(2);
			ImGui::PopFont();

			// Progress indicator
			static auto startTime = std::chrono::steady_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::steady_clock::now() - startTime).count();

			ImGui::Text("Waiting for confirmation... (%lds)", elapsed);

			if (elapsed > 300) { // 5 minute timeout
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: This prompt will timeout soon!");
			}
		}
	}
	else {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.2f, 1.0f)); // Green button
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.6f, 0.1f, 1.0f));

		if (ImGui::Button("Start Process", ImVec2(-1, 50))) {
			StartProcess(m_selectedProcess);
		}

		ImGui::PopStyleColor(3);
	}

	// Add a debug section to show device connection status
	if (ImGui::CollapsingHeader("Device Connection Status")) {
		std::vector<std::string> devices = { "gantry-main", "hex-left", "hex-right" };

		for (const auto& deviceName : devices) {
			bool isConnected = m_machineOps.IsDeviceConnected(deviceName);
			ImGui::Text("%s: %s", deviceName.c_str(),
				isConnected ? "Connected" : "Not Connected");
		}
	}

	// Add help text at the bottom
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Tip: Right-click any process button to see detailed steps");

	ImGui::End();
}
void ProcessControlPanel::StartProcess(const std::string& processName) {
	if (m_processRunning) {
		m_logger->LogWarning("ProcessControlPanel: Process already running");
		return;
	}

	m_processRunning = true;
	m_progress = 0.0f;
	m_stopRequested = false;

	UpdateStatus("Starting process: " + processName);

	// Start the process in a separate thread
	m_processThread = std::thread(&ProcessControlPanel::ProcessThreadFunc, this, processName);
	m_processThread.detach(); // Let it run independently
}

void ProcessControlPanel::StopProcess() {
	if (!m_processRunning) {
		return;
	}

	UpdateStatus("Stopping process...");
	m_stopRequested = true;

	// Wait for the process to complete (with a timeout)
	int timeoutCounter = 0;
	while (m_processRunning && timeoutCounter < 100) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		timeoutCounter++;
	}

	// If it's still running, we assume it's hung and just set the flag
	if (m_processRunning) {
		m_processRunning = false;
		UpdateStatus("Process forcibly terminated", true);
	}
	else {
		UpdateStatus("Process stopped");
	}
}

void ProcessControlPanel::UpdateStatus(const std::string& message, bool isError) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_statusMessage = message;

	if (isError) {
		m_logger->LogError("ProcessControlPanel: " + message);
	}
	else {
		m_logger->LogInfo("ProcessControlPanel: " + message);
	}
}

// In ProcessControlPanel.cpp, add these cases to the BuildSelectedProcess method:

std::unique_ptr<SequenceStep> ProcessControlPanel::BuildSelectedProcess() {
	if (m_selectedProcess == "Initialization") {
		return ProcessBuilders::BuildInitializationSequence(m_machineOps);
	}
	else if (m_selectedProcess == "InitializationParallel") {
		return ProcessBuilders::BuildInitializationSequenceParallel(m_machineOps);
	}
	else if (m_selectedProcess == "Probing") {
		return ProcessBuilders::BuildProbingSequence(m_machineOps, *m_uiManager);
	}
	else if (m_selectedProcess == "PickPlaceLeftLens") {
		return ProcessBuilders::BuildPickPlaceLeftLensSequence(m_machineOps, *m_uiManager);
	}
	else if (m_selectedProcess == "PickPlaceRightLens") {
		return ProcessBuilders::BuildPickPlaceRightLensSequence(m_machineOps, *m_uiManager);
	}
	else if (m_selectedProcess == "UVCuring") {
		return ProcessBuilders::BuildUVCuringSequence(m_machineOps, *m_uiManager);
	}
	else if (m_selectedProcess == "CompleteProcess") {
		return ProcessBuilders::BuildCompleteProcessSequence(m_machineOps, *m_uiManager);
	}
	else if (m_selectedProcess == "RejectLeftLens") {
		return ProcessBuilders::RejectLeftLensSequence(m_machineOps, *m_uiManager);
	}
	else if (m_selectedProcess == "RejectRightLens") {
		return ProcessBuilders::RejectRightLensSequence(m_machineOps, *m_uiManager);
	}
	// ADD this new case for needle calibration
	else if (m_selectedProcess == "NeedleCalibration") {
		return ProcessBuilders::BuildNeedleXYCalibrationSequenceEnhanced(m_machineOps, *m_uiManager);
	}
	else if (m_selectedProcess == "DispenseCalibration1") {
		return ProcessBuilders::BuildDispenseCalibrationSequence(m_machineOps, *m_uiManager);
	}
	else if (m_selectedProcess == "DispenseCalibration2") {
		return ProcessBuilders::BuildDispenseCalibration2Sequence(m_machineOps, *m_uiManager);
	}
	else if (m_selectedProcess == "DispenseEpoxy1") {
		return ProcessBuilders::BuildDispenseEpoxy1Sequence(m_machineOps, *m_uiManager);
	}
	else if (m_selectedProcess == "DispenseEpoxy2") {
		return ProcessBuilders::BuildDispenseEpoxy2Sequence(m_machineOps, *m_uiManager);
	}


	// Default fallback
	return ProcessBuilders::BuildInitializationSequence(m_machineOps);
}

void ProcessControlPanel::ProcessThreadFunc(const std::string& processName) {
	try {
		// Build the process
		std::unique_ptr<SequenceStep> sequence = BuildSelectedProcess();
		if (!sequence) {
			UpdateStatus("Failed to create process: " + processName, true);
			m_processRunning = false;
			return;
		}

		// Number of operations to track progress
		const auto& operations = sequence->GetOperations();
		size_t totalOperations = operations.size();
		size_t completedOperations = 0;

		// Set up step completion tracking
		bool processSuccess = false;
		bool processComplete = false;

		sequence->SetCompletionCallback([&](bool success) {
			processSuccess = success;
			processComplete = true;
		});

		// Create a progress tracking wrapper class for operations
		class ProgressTrackingOperation : public SequenceOperation {
		public:
			ProgressTrackingOperation(std::shared_ptr<SequenceOperation> original,
				ProcessControlPanel* panel, size_t& completed, size_t total)
				: m_original(original), m_panel(panel),
				m_completedOps(completed), m_totalOps(total) {
			}

			bool Execute(MachineOperations& ops) override {
				m_panel->UpdateStatus("Executing: " + GetDescription());
				bool result = m_original->Execute(ops);
				m_completedOps++;
				// Update progress (thread-safe because atomic)
				m_panel->m_progress = static_cast<float>(m_completedOps) / m_totalOps;
				return result && !m_panel->m_stopRequested;
			}

			std::string GetDescription() const override {
				return m_original->GetDescription();
			}

		private:
			std::shared_ptr<SequenceOperation> m_original;
			ProcessControlPanel* m_panel;
			size_t& m_completedOps;
			size_t m_totalOps;
		};

		// Replace each operation with a tracking wrapper
		std::vector<std::shared_ptr<SequenceOperation>> trackedOperations;
		for (const auto& operation : operations) {
			trackedOperations.push_back(
				std::make_shared<ProgressTrackingOperation>(
					operation, this, completedOperations, totalOperations
				)
			);
		}

		// Create a new sequence with the tracked operations
		auto trackingSequence = std::make_unique<SequenceStep>("Tracking:" + processName, m_machineOps);
		for (const auto& op : trackedOperations) {
			trackingSequence->AddOperation(op);
		}

		// Set the completion callback on the new sequence
		trackingSequence->SetCompletionCallback([&](bool success) {
			processSuccess = success;
			processComplete = true;
		});

		// Replace the original sequence with our tracking sequence
		sequence = std::move(trackingSequence);

		// Execute the sequence
		UpdateStatus("Executing process: " + processName);
		sequence->Execute();

		// Wait for completion callback or timeout
		int timeoutMs = 60000; // 60 second timeout
		auto startTime = std::chrono::steady_clock::now();

		while (!processComplete) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			// Check for timeout
			auto currentTime = std::chrono::steady_clock::now();
			auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
				currentTime - startTime).count();

			if (elapsedMs > timeoutMs || m_stopRequested) {
				UpdateStatus("Process timed out or was stopped", true);
				break;
			}
		}

		// Update status based on process result
		if (processComplete) {
			if (processSuccess) {
				UpdateStatus("Process completed successfully");
			}
			else {
				UpdateStatus("Process failed", true);
			}
		}
	}
	catch (const std::exception& e) {
		UpdateStatus("Exception during process execution: " + std::string(e.what()), true);
	}
	catch (...) {
		UpdateStatus("Unknown exception during process execution", true);
	}

	// Always mark process as not running when we exit
	m_processRunning = false;
	m_progress = 0.0f;
}