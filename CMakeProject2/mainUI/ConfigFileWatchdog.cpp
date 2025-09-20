#include "ConfigFileWatchdog.h"
#include <iostream>
#include <algorithm>
#include "imgui.h"

ConfigFileWatchdog::ConfigFileWatchdog(int pollIntervalMs, bool autoUpdateDb, Logger* logger)
	: m_pollInterval(pollIntervalMs)
	, m_autoUpdateDatabase(autoUpdateDb)
	, m_logger(logger)
	, m_running(false)
	, m_changesDetected(0)
	, m_databaseUpdates(0)
	, m_updateFailures(0)
	, m_startTime(std::chrono::system_clock::now())
{
	if (m_logger) {
		m_logger->LogInfo("ConfigFileWatchdog created - Poll interval: " + std::to_string(pollIntervalMs) + "ms");
	}
}

ConfigFileWatchdog::~ConfigFileWatchdog() {
	Stop();
}

bool ConfigFileWatchdog::AddFile(const std::string& filepath) {
	std::lock_guard<std::mutex> lock(m_filesMutex);

	// Check if already watching this file
	for (const auto& file : m_watchedFiles) {
		if (file.filepath == filepath) {
			if (m_logger) {
				m_logger->LogWarning("File already being watched: " + filepath);
			}
			return false;
		}
	}

	// Check if file exists and get initial state
	WatchedFile watchedFile;
	watchedFile.filepath = filepath;
	watchedFile.lastChecked = std::chrono::system_clock::now();

	if (std::filesystem::exists(filepath)) {
		watchedFile.exists = true;
		watchedFile.lastModified = std::filesystem::last_write_time(filepath);

		if (m_logger) {
			m_logger->LogInfo("Added file to watchdog: " + filepath);
		}
	}
	else {
		watchedFile.exists = false;

		if (m_logger) {
			m_logger->LogWarning("File does not exist but added to watchdog: " + filepath);
		}
	}

	m_watchedFiles.push_back(watchedFile);
	return true;
}

int ConfigFileWatchdog::AddFiles(const std::vector<std::string>& filepaths) {
	int addedCount = 0;
	for (const std::string& filepath : filepaths) {
		if (AddFile(filepath)) {
			addedCount++;
		}
	}

	if (m_logger) {
		m_logger->LogInfo("Added " + std::to_string(addedCount) + " out of " +
			std::to_string(filepaths.size()) + " files to watchdog");
	}

	return addedCount;
}

bool ConfigFileWatchdog::RemoveFile(const std::string& filepath) {
	std::lock_guard<std::mutex> lock(m_filesMutex);

	auto it = std::find_if(m_watchedFiles.begin(), m_watchedFiles.end(),
		[&filepath](const WatchedFile& file) {
		return file.filepath == filepath;
	});

	if (it != m_watchedFiles.end()) {
		m_watchedFiles.erase(it);
		if (m_logger) {
			m_logger->LogInfo("Removed file from watchdog: " + filepath);
		}
		return true;
	}

	return false;
}

void ConfigFileWatchdog::ClearFiles() {
	std::lock_guard<std::mutex> lock(m_filesMutex);
	m_watchedFiles.clear();

	if (m_logger) {
		m_logger->LogInfo("Cleared all files from watchdog");
	}
}

void ConfigFileWatchdog::SetPollInterval(int pollIntervalMs) {
	m_pollInterval = std::chrono::milliseconds(pollIntervalMs);

	if (m_logger) {
		m_logger->LogInfo("Watchdog poll interval set to: " + std::to_string(pollIntervalMs) + "ms");
	}
}

void ConfigFileWatchdog::SetAutoUpdateDatabase(bool autoUpdate) {
	m_autoUpdateDatabase = autoUpdate;

	if (m_logger) {
		m_logger->LogInfo("Watchdog auto-update database: " + std::string(autoUpdate ? "enabled" : "disabled"));
	}
}

void ConfigFileWatchdog::SetLogger(Logger* logger) {
	m_logger = logger;
}

bool ConfigFileWatchdog::Start() {
	if (m_running) {
		if (m_logger) {
			m_logger->LogWarning("Watchdog is already running");
		}
		return false;
	}

	// Check if we have files to watch
	{
		std::lock_guard<std::mutex> lock(m_filesMutex);
		if (m_watchedFiles.empty()) {
			if (m_logger) {
				m_logger->LogWarning("No files to watch - add files before starting");
			}
			return false;
		}
	}

	// Reset statistics
	m_changesDetected = 0;
	m_databaseUpdates = 0;
	m_updateFailures = 0;
	m_startTime = std::chrono::system_clock::now();

	// Start the watch thread
	m_running = true;
	m_watchThread = std::make_unique<std::thread>(&ConfigFileWatchdog::WatchLoop, this);

	if (m_logger) {
		std::lock_guard<std::mutex> lock(m_filesMutex);
		m_logger->LogInfo("ConfigFileWatchdog started - monitoring " +
			std::to_string(m_watchedFiles.size()) + " files");
	}

	return true;
}

void ConfigFileWatchdog::Stop() {
	if (!m_running) {
		return;
	}

	m_running = false;

	if (m_watchThread && m_watchThread->joinable()) {
		m_watchThread->join();
		m_watchThread.reset();
	}

	if (m_logger) {
		auto duration = std::chrono::system_clock::now() - m_startTime;
		auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count();

		m_logger->LogInfo("ConfigFileWatchdog stopped after " + std::to_string(minutes) + " minutes");
		m_logger->LogInfo("Statistics: " + std::to_string(m_changesDetected.load()) + " changes, " +
			std::to_string(m_databaseUpdates.load()) + " DB updates, " +
			std::to_string(m_updateFailures.load()) + " failures");
	}
}

bool ConfigFileWatchdog::IsRunning() const {
	return m_running;
}

void ConfigFileWatchdog::WatchLoop() {
	if (m_logger) {
		m_logger->LogInfo("Watchdog thread started with " + std::to_string(m_pollInterval.count()) + "ms interval");
	}

	while (m_running) {
		try {
			// Check all files
			ForceCheck();

			// Sleep for poll interval
			std::this_thread::sleep_for(m_pollInterval);
		}
		catch (const std::exception& e) {
			if (m_logger) {
				m_logger->LogError("Watchdog thread error: " + std::string(e.what()));
			}
			// Continue running despite errors
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

	if (m_logger) {
		m_logger->LogInfo("Watchdog thread stopped");
	}
}

bool ConfigFileWatchdog::CheckFileChanged(WatchedFile& file) {
	auto now = std::chrono::system_clock::now();
	file.lastChecked = now;

	// Check if file exists now
	bool existsNow = std::filesystem::exists(file.filepath);

	if (!file.exists && existsNow) {
		// File was created
		file.exists = true;
		file.lastModified = std::filesystem::last_write_time(file.filepath);
		return true;
	}
	else if (file.exists && !existsNow) {
		// File was deleted
		file.exists = false;
		return true;
	}
	else if (file.exists && existsNow) {
		// File exists - check modification time
		auto currentModified = std::filesystem::last_write_time(file.filepath);
		if (currentModified != file.lastModified) {
			file.lastModified = currentModified;
			return true;
		}
	}

	return false;
}

void ConfigFileWatchdog::HandleFileChange(const WatchedFile& file) {
	FileChangeEvent event;
	event.filename = file.filepath;
	event.lastModified = file.lastModified;
	event.detectedAt = std::chrono::system_clock::now();

	if (m_logger) {
		if (file.exists) {
			m_logger->LogInfo("🔄 File changed detected: " + file.filepath);
		}
		else {
			m_logger->LogInfo("🗑️ File deleted detected: " + file.filepath);
		}
	}

	// Update database if auto-update is enabled and file exists
	if (m_autoUpdateDatabase && file.exists) {
		if (ConfigDatabaseUtils::IsDatabaseInitialized()) {
			bool success = ConfigDatabaseUtils::SaveConfigToDatabase(file.filepath, true, m_logger);
			event.updateSuccess = success;

			if (success) {
				m_databaseUpdates++;
				if (m_logger) {
					m_logger->LogInfo("✅ Database updated for: " + file.filepath);
				}
			}
			else {
				m_updateFailures++;
				event.errorMessage = "Failed to update database";
				if (m_logger) {
					m_logger->LogError("❌ Failed to update database for: " + file.filepath);
				}
			}
		}
		else {
			event.errorMessage = "Database not initialized";
			m_updateFailures++;
			if (m_logger) {
				m_logger->LogWarning("⚠️ Database not available for update: " + file.filepath);
			}
		}
	}

	// Add to recent events
	AddToRecentEvents(event);

	// Notify callbacks
	NotifyCallbacks(event);

	m_changesDetected++;
}

void ConfigFileWatchdog::NotifyCallbacks(const FileChangeEvent& event) {
	std::lock_guard<std::mutex> lock(m_callbacksMutex);

	for (const auto& callback : m_callbacks) {
		try {
			callback(event);
		}
		catch (const std::exception& e) {
			if (m_logger) {
				m_logger->LogError("Error in watchdog callback: " + std::string(e.what()));
			}
		}
	}
}

void ConfigFileWatchdog::AddChangeCallback(const FileChangeCallback& callback) {
	std::lock_guard<std::mutex> lock(m_callbacksMutex);
	m_callbacks.push_back(callback);
}

void ConfigFileWatchdog::ClearCallbacks() {
	std::lock_guard<std::mutex> lock(m_callbacksMutex);
	m_callbacks.clear();
}

std::vector<std::string> ConfigFileWatchdog::GetWatchedFiles() const {
	std::lock_guard<std::mutex> lock(m_filesMutex);

	std::vector<std::string> filenames;
	for (const auto& file : m_watchedFiles) {
		filenames.push_back(file.filepath);
	}

	return filenames;
}

nlohmann::json ConfigFileWatchdog::GetStatistics() const {
	auto duration = std::chrono::system_clock::now() - m_startTime;
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count();

	nlohmann::json stats;
	stats["running"] = m_running.load();
	stats["uptime_minutes"] = minutes;
	stats["poll_interval_ms"] = m_pollInterval.count();
	stats["auto_update_database"] = m_autoUpdateDatabase;
	stats["watched_files_count"] = GetWatchedFiles().size();
	stats["changes_detected"] = m_changesDetected.load();
	stats["database_updates"] = m_databaseUpdates.load();
	stats["update_failures"] = m_updateFailures.load();

	// Add file status
	std::lock_guard<std::mutex> lock(m_filesMutex);
	nlohmann::json filesArray = nlohmann::json::array();
	for (const auto& file : m_watchedFiles) {
		nlohmann::json fileInfo;
		fileInfo["path"] = file.filepath;
		fileInfo["exists"] = file.exists;
		if (file.exists) {
			// Convert file_time_type to string representation
			auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
				file.lastModified - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
			auto time_t = std::chrono::system_clock::to_time_t(sctp);

			char timeBuffer[26];
			ctime_s(timeBuffer, sizeof(timeBuffer), &time_t);
			fileInfo["last_modified"] = timeBuffer;
		}
		filesArray.push_back(fileInfo);
	}
	stats["files"] = filesArray;

	return stats;
}

int ConfigFileWatchdog::ForceCheck() {
	std::lock_guard<std::mutex> lock(m_filesMutex);

	int changesFound = 0;
	for (auto& file : m_watchedFiles) {
		if (CheckFileChanged(file)) {
			HandleFileChange(file);
			changesFound++;
		}
	}

	return changesFound;
}

std::vector<ConfigFileWatchdog::FileChangeEvent> ConfigFileWatchdog::GetRecentEvents(int maxEvents) const {
	std::lock_guard<std::mutex> lock(m_eventsMutex);

	std::vector<FileChangeEvent> result;
	int count = std::min(maxEvents, static_cast<int>(m_recentEvents.size()));

	// Return most recent events
	if (count > 0) {
		result.assign(m_recentEvents.end() - count, m_recentEvents.end());
	}

	return result;
}

void ConfigFileWatchdog::AddToRecentEvents(const FileChangeEvent& event) {
	std::lock_guard<std::mutex> lock(m_eventsMutex);

	m_recentEvents.push_back(event);

	// Keep only the most recent events
	if (m_recentEvents.size() > MAX_RECENT_EVENTS) {
		m_recentEvents.erase(m_recentEvents.begin());
	}
}

// Static utility methods
std::unique_ptr<ConfigFileWatchdog> ConfigFileWatchdog::CreateForUAA3Configs(Logger* logger) {
	auto watchdog = std::make_unique<ConfigFileWatchdog>(1000, true, logger);

	// Add common UAA3 config files
	std::vector<std::string> uaa3Configs = {
			"motion_config.json",
			"IOConfig.json",
			"DataServerConfig.json",
			"camera_config.json",
			"smu_config.json",
			"default_config.json"
	};

	watchdog->AddFiles(uaa3Configs);

	return watchdog;
}

std::unique_ptr<ConfigFileWatchdog> ConfigFileWatchdog::StartUAA3Watchdog(Logger* logger) {
	auto watchdog = CreateForUAA3Configs(logger);

	if (watchdog->Start()) {
		if (logger) {
			logger->LogInfo("UAA3 Config Watchdog started successfully");
		}
	}
	else {
		if (logger) {
			logger->LogError("Failed to start UAA3 Config Watchdog");
		}
	}

	return watchdog;
}

// Optional: Add this to your MainUIManager or create a separate UI class

/**
 * @brief Render watchdog status in ImGui (add this to your UI rendering)
 */
void RenderWatchdogStatus(ConfigFileWatchdog* watchdog) {
	if (!watchdog) {
		return;
	}

	// Create a collapsible section in your UI
	if (ImGui::CollapsingHeader("📂 Configuration File Watchdog")) {
		auto stats = watchdog->GetStatistics();

		// Status indicators
		bool running = stats["running"].get<bool>();
		if (running) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● RUNNING");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "● STOPPED");
		}

		ImGui::SameLine();
		ImGui::Text("Uptime: %d minutes", stats["uptime_minutes"].get<int>());

		// Statistics
		ImGui::Separator();
		ImGui::Text("Statistics:");
		ImGui::BulletText("Changes detected: %d", stats["changes_detected"].get<int>());
		ImGui::BulletText("Database updates: %d", stats["database_updates"].get<int>());
		ImGui::BulletText("Update failures: %d", stats["update_failures"].get<int>());
		ImGui::BulletText("Poll interval: %d ms", stats["poll_interval_ms"].get<int>());

		// Watched files
		ImGui::Separator();
		ImGui::Text("Watched Files (%d):", stats["watched_files_count"].get<int>());

		if (stats.contains("files") && stats["files"].is_array()) {
			for (const auto& file : stats["files"]) {
				std::string filename = file["path"].get<std::string>();
				bool exists = file["exists"].get<bool>();

				if (exists) {
					ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓");
				}
				else {
					ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗");
				}
				ImGui::SameLine();
				ImGui::Text("%s", filename.c_str());

				if (exists && file.contains("last_modified")) {
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
						"(%s)", file["last_modified"].get<std::string>().c_str());
				}
			}
		}

		// Recent events
		ImGui::Separator();
		if (ImGui::TreeNode("Recent Events")) {
			auto recentEvents = watchdog->GetRecentEvents(5);

			if (recentEvents.empty()) {
				ImGui::Text("No recent events");
			}
			else {
				for (const auto& event : recentEvents) {
					auto timePoint = event.detectedAt;
					auto time_t = std::chrono::system_clock::to_time_t(timePoint);
					char timeBuffer[26]; // ctime_s requires a 26-char buffer
					ctime_s(timeBuffer, sizeof(timeBuffer), &time_t);
					std::string timeStr(timeBuffer);
					timeStr.pop_back(); // Remove newline

					if (event.updateSuccess) {
						ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓");
					}
					else {
						ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗");
					}
					ImGui::SameLine();
					ImGui::Text("%s - %s", event.filename.c_str(), timeStr.c_str());

					if (!event.errorMessage.empty()) {
						ImGui::SameLine();
						ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
							"(%s)", event.errorMessage.c_str());
					}
				}
			}
			ImGui::TreePop();
		}

		// Control buttons
		ImGui::Separator();
		if (running) {
			if (ImGui::Button("Stop Watchdog")) {
				watchdog->Stop();
			}
		}
		else {
			if (ImGui::Button("Start Watchdog")) {
				watchdog->Start();
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Force Check Now")) {
			int changes = watchdog->ForceCheck();
			// Could show a status message here
		}

		ImGui::SameLine();
		if (ImGui::Button("Clear Events")) {
			// This would require adding a ClearRecentEvents() method to the watchdog
		}
	}
}
