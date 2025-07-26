#pragma once

#include "ConfigDatabaseUtils.h"
#include "include/logger.h"
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

/**
 * @brief File system watchdog for monitoring JSON configuration files
 *
 * Monitors JSON configuration files for changes and automatically updates
 * the database when files are modified by the running program or external editors.
 */
class ConfigFileWatchdog {
public:
    /**
     * @brief File change event information
     */
    struct FileChangeEvent {
        std::string filename;
        std::filesystem::file_time_type lastModified;
        std::chrono::system_clock::time_point detectedAt;
        bool updateSuccess = false;
        std::string errorMessage;
    };

    /**
     * @brief Callback function type for file change notifications
     * @param event The file change event that occurred
     */
    using FileChangeCallback = std::function<void(const FileChangeEvent& event)>;

private:
    struct WatchedFile {
        std::string filepath;
        std::filesystem::file_time_type lastModified;
        bool exists = false;
        std::chrono::system_clock::time_point lastChecked;
    };

    // Configuration
    std::vector<WatchedFile> m_watchedFiles;
    std::chrono::milliseconds m_pollInterval;
    bool m_autoUpdateDatabase;
    Logger* m_logger;

    // Threading
    std::atomic<bool> m_running;
    std::unique_ptr<std::thread> m_watchThread;
    mutable std::mutex m_filesMutex;

    // Callbacks
    std::vector<FileChangeCallback> m_callbacks;
    mutable std::mutex m_callbacksMutex;

    // Statistics
    std::atomic<int> m_changesDetected;
    std::atomic<int> m_databaseUpdates;
    std::atomic<int> m_updateFailures;
    std::chrono::system_clock::time_point m_startTime;

    // Internal methods
    void WatchLoop();
    bool CheckFileChanged(WatchedFile& file);
    void HandleFileChange(const WatchedFile& file);
    void NotifyCallbacks(const FileChangeEvent& event);

public:
    /**
     * @brief Constructor
     * @param pollIntervalMs How often to check files in milliseconds (default: 1000ms)
     * @param autoUpdateDb Whether to automatically update database on changes (default: true)
     * @param logger Logger instance for reporting (optional)
     */
    ConfigFileWatchdog(int pollIntervalMs = 1000, bool autoUpdateDb = true, Logger* logger = nullptr);

    /**
     * @brief Destructor - stops watchdog if running
     */
    ~ConfigFileWatchdog();

    // Configuration methods

    /**
     * @brief Add a file to watch
     * @param filepath Path to the JSON file to monitor
     * @return true if file was added successfully
     */
    bool AddFile(const std::string& filepath);

    /**
     * @brief Add multiple files to watch
     * @param filepaths Vector of file paths to monitor
     * @return Number of files successfully added
     */
    int AddFiles(const std::vector<std::string>& filepaths);

    /**
     * @brief Remove a file from watching
     * @param filepath Path to stop monitoring
     * @return true if file was being watched and removed
     */
    bool RemoveFile(const std::string& filepath);

    /**
     * @brief Clear all watched files
     */
    void ClearFiles();

    /**
     * @brief Set poll interval
     * @param pollIntervalMs Polling interval in milliseconds
     */
    void SetPollInterval(int pollIntervalMs);

    /**
     * @brief Enable/disable automatic database updates
     * @param autoUpdate Whether to automatically update database
     */
    void SetAutoUpdateDatabase(bool autoUpdate);

    /**
     * @brief Set logger instance
     * @param logger Logger for reporting events
     */
    void SetLogger(Logger* logger);

    // Control methods

    /**
     * @brief Start the watchdog
     * @return true if started successfully
     */
    bool Start();

    /**
     * @brief Stop the watchdog
     */
    void Stop();

    /**
     * @brief Check if watchdog is running
     * @return true if currently monitoring files
     */
    bool IsRunning() const;

    // Callback management

    /**
     * @brief Add a callback for file change events
     * @param callback Function to call when files change
     */
    void AddChangeCallback(const FileChangeCallback& callback);

    /**
     * @brief Remove all change callbacks
     */
    void ClearCallbacks();

    // Information methods

    /**
     * @brief Get list of currently watched files
     * @return Vector of file paths being monitored
     */
    std::vector<std::string> GetWatchedFiles() const;

    /**
     * @brief Get watchdog statistics
     * @return JSON object with statistics
     */
    nlohmann::json GetStatistics() const;

    /**
     * @brief Force check all files now (manual trigger)
     * @return Number of changes detected
     */
    int ForceCheck();

    /**
     * @brief Get the last change events (up to maxEvents)
     * @param maxEvents Maximum number of recent events to return
     * @return Vector of recent file change events
     */
    std::vector<FileChangeEvent> GetRecentEvents(int maxEvents = 10) const;

    // Utility methods

    /**
     * @brief Create a watchdog for common UAA3 config files
     * @param logger Logger instance (optional)
     * @return Configured watchdog ready to start
     */
    static std::unique_ptr<ConfigFileWatchdog> CreateForUAA3Configs(Logger* logger = nullptr);

    /**
     * @brief Quick setup and start watching UAA3 config files
     * @param logger Logger instance (optional)
     * @return Pointer to started watchdog (caller owns)
     */
    static std::unique_ptr<ConfigFileWatchdog> StartUAA3Watchdog(Logger* logger = nullptr);

private:
    // Recent events tracking (circular buffer)
    mutable std::mutex m_eventsMutex;
    std::vector<FileChangeEvent> m_recentEvents;
    static const size_t MAX_RECENT_EVENTS = 50;
    void AddToRecentEvents(const FileChangeEvent& event);
};