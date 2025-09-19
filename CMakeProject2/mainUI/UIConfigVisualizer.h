// UIConfigVisualizer.h - Updated with Broadcasting Pattern
#pragma once

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include "include/camera/CameraFrameData.h"
#include "include/machine_operations.h"
#include "include/motions/MotionConfigManager.h"
#include "include/motions/MotionTypes.h"
#include "include/logger.h"
#include "UIConfigVisualizerPositionSubscriber.h"
#include "imgui.h"
#include <SDL_opengl.h>  // For OpenGL texture management

// Forward declarations
class NodePropertiesHandler;
class CameraManager;
class MachineOperations;
class CameraFrameSubscriber;

/**
 * @brief Camera feed subscriber for UIConfigVisualizer
 *
 * This subscriber receives camera frames via the broadcasting system
 * and renders them in the configuration visualizer UI.
 */
class UIConfigVisualizerSubscriber : public CameraFrameSubscriber {
public:
	explicit UIConfigVisualizerSubscriber(const std::string& cameraId);
	~UIConfigVisualizerSubscriber();

	// Disable copy/move
	UIConfigVisualizerSubscriber(const UIConfigVisualizerSubscriber&) = delete;
	UIConfigVisualizerSubscriber& operator=(const UIConfigVisualizerSubscriber&) = delete;
	UIConfigVisualizerSubscriber(UIConfigVisualizerSubscriber&&) = delete;
	UIConfigVisualizerSubscriber& operator=(UIConfigVisualizerSubscriber&&) = delete;

	// CameraFrameSubscriber interface
	void OnNewFrame(const CameraFrameData& frameData) override;
	void OnCameraStatusChanged(const std::string& cameraId, bool connected, bool grabbing) override;
	std::string GetSubscriberId() const override;
	bool WantsFramesFromCamera(const std::string& cameraId) const override;
	int GetMinFrameIntervalMs() const override { return 50; } // 20fps for UI config

	// Camera management
	void SetTargetCamera(const std::string& cameraId);
	std::string GetTargetCamera() const { return m_targetCameraId; }

	// Frame access (thread-safe)
	bool HasNewFrame() const { return m_hasNewFrame.load(); }
	CameraFrameData GetLatestFrame() const;
	void MarkFrameConsumed() { m_hasNewFrame.store(false); }

	// Status access (thread-safe)
	bool IsCameraConnected() const { return m_cameraConnected.load(); }
	bool IsCameraGrabbing() const { return m_cameraGrabbing.load(); }
	uint64_t GetTotalFramesReceived() const { return m_totalFramesReceived.load(); }
	uint64_t GetLastFrameTimestamp() const { return m_lastFrameTimestamp.load(); }

	// OpenGL texture access
	unsigned int GetTextureID() const { return m_textureID; }
	bool HasValidTexture() const { return m_textureInitialized && m_textureWidth > 0 && m_textureHeight > 0; }
	uint32_t GetTextureWidth() const { return m_textureWidth; }
	uint32_t GetTextureHeight() const { return m_textureHeight; }

	// Texture management (must be called from main thread)
	void UpdateTextureIfNeeded();
	void CleanupTexture();

private:
	// Identification
	std::string m_subscriberId;
	std::string m_targetCameraId;

	// Frame data (protected by mutex)
	mutable std::mutex m_frameMutex;
	CameraFrameData m_latestFrame;
	std::atomic<bool> m_hasNewFrame{ false };

	// Status tracking (atomic for thread safety)
	std::atomic<bool> m_cameraConnected{ false };
	std::atomic<bool> m_cameraGrabbing{ false };
	std::atomic<uint64_t> m_totalFramesReceived{ 0 };
	std::atomic<uint64_t> m_lastFrameTimestamp{ 0 };

	// OpenGL texture management
	unsigned int m_textureID = 0;
	bool m_textureInitialized = false;
	uint32_t m_textureWidth = 0;
	uint32_t m_textureHeight = 0;

	// Texture update tracking
	std::atomic<bool> m_needsTextureUpdate{ false };
	CameraFrameData m_pendingTextureFrame;
	mutable std::mutex m_textureMutex;

	// Helper methods
	void ResetState();
	void UpdateSubscriberId();
	void CreateOrUpdateTexture(const CameraFrameData& frameData);
};

class UIConfigVisualizer {
public:
	UIConfigVisualizer(MotionConfigManager& configManager, CameraManager* cameraManager = nullptr);
	~UIConfigVisualizer();

	// Disable copy/move to avoid issues with incomplete types
	UIConfigVisualizer(const UIConfigVisualizer&) = delete;
	UIConfigVisualizer& operator=(const UIConfigVisualizer&) = delete;
	UIConfigVisualizer(UIConfigVisualizer&&) = delete;
	UIConfigVisualizer& operator=(UIConfigVisualizer&&) = delete;

	// UI rendering - no window wrapper, just content
	void RenderUI();
	void ToggleWindow();
	bool IsVisible() const { return showWindow; }

	// Set the active graph to visualize
	void SetActiveGraph(const std::string& graphName);

	// Set machine operations for movement functionality
	void SetMachineOperations(MachineOperations* machineOps);

	// Camera broadcasting management
	void InitializeCameraFeed();
	void SetSelectedCamera(const std::string& cameraId);
	void ClearCameraFeed();

private:
	// Reference to the config manager and logger
	MotionConfigManager& configManager;
	Logger* m_logger;

	// Camera manager (optional)
	CameraManager* m_cameraManager;

	// Machine operations (optional)
	MachineOperations* m_machineOperations;

	// Broadcasting-based camera feed
	std::shared_ptr<UIConfigVisualizerSubscriber> m_cameraSubscriber;
	std::string m_selectedCameraId;
	bool m_cameraSystemInitialized = false;

	// Action handlers
	std::unique_ptr<NodePropertiesHandler> m_propertiesHandler;

	// UI state
	bool showWindow = true;
	std::string m_activeGraph;
	float m_zoomLevel = 1.0f;
	ImVec2 m_panOffset = ImVec2(0, 0);
	bool m_isCanvasHovered = false;

	// Node selection and interaction state
	bool m_isDragging = false;
	std::string m_draggedNodeId;
	ImVec2 m_dragStartPos;
	ImVec2 m_lastMousePos;

	// Selected node state (replaces context menu)
	std::string m_selectedNodeId;
	bool m_showNodeActions = false;

	// Rendering constants
	static constexpr float NODE_WIDTH = 160.0f;
	static constexpr float NODE_HEIGHT = 80.0f;
	static constexpr float NODE_ROUNDING = 5.0f;
	static constexpr ImU32 NODE_COLOR = IM_COL32(70, 70, 200, 255);
	static constexpr ImU32 NODE_BORDER_COLOR = IM_COL32(255, 255, 255, 255);
	static constexpr ImU32 SELECTED_NODE_COLOR = IM_COL32(120, 120, 255, 255);
	static constexpr ImU32 EDGE_COLOR = IM_COL32(200, 200, 200, 255);
	static constexpr ImU32 EDGE_HOVER_COLOR = IM_COL32(250, 250, 100, 255);
	static constexpr ImU32 BIDIRECTIONAL_EDGE_COLOR = IM_COL32(50, 205, 50, 255);
	static constexpr ImU32 ARROW_COLOR = IM_COL32(220, 220, 220, 255);
	static constexpr float ARROW_SIZE = 10.0f;
	static constexpr float EDGE_THICKNESS = 2.0f;
	static constexpr float TEXT_PADDING = 5.0f;

	// Main rendering methods - implemented in UIConfigVisualizer_Graph.cpp
	void RenderGraphControls();
	void RenderGraphCanvas();
	void RenderLeftPanel();
	void RenderCameraCanvas(float height);
	void RenderBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize);
	void RenderDevicePositionsPanel();
	void RenderNodes(ImDrawList* drawList, const ImVec2& canvasPos);
	void RenderEdges(ImDrawList* drawList, const ImVec2& canvasPos);

	// Enhanced camera rendering methods
	void RenderCameraFeedFromSubscriber(ImVec2 canvasSize);
	void RenderCameraPlaceholder(ImVec2 canvasSize, const std::string& message);
	void RenderCameraControls();

	// Input handling methods - implemented in UIConfigVisualizer_Input.cpp
	void HandleInput(const ImVec2& canvasPos, const ImVec2& canvasSize);
	void HandleZooming();
	void HandlePanning();
	void HandleNodeDragging(const ImVec2& canvasPos);
	void HandleNodeSelection(const ImVec2& canvasPos);

	// Helper methods - implemented in UIConfigVisualizer_Helpers.cpp
	ImVec2 GraphToCanvas(const ImVec2& pos, const ImVec2& canvasPos) const;
	ImVec2 CanvasToGraph(const ImVec2& pos, const ImVec2& canvasPos) const;
	ImVec2 GetNodePosition(const Node& node) const;
	void SaveNodePosition(const std::string& nodeId, const ImVec2& newPos);
	void DrawArrow(ImDrawList* drawList, const ImVec2& start, const ImVec2& end, ImU32 color, float thickness, bool isBidirectional = false);
	std::string GetNodeAtPosition(const ImVec2& pos, const ImVec2& canvasPos);
	std::string GetDeviceCurrentNodeName(const std::string& deviceName);

	// Position caching for performance optimization
	std::map<std::string, std::string> m_cachedPositionNames;
	std::chrono::steady_clock::time_point m_lastPositionNameUpdate;
	static constexpr std::chrono::milliseconds POSITION_NAME_CACHE_TIMEOUT{ 500 }; // 500ms cache

	// Helper method declaration
	void RefreshPositionNames();

	// Position subscriber for real-time updates
	std::shared_ptr<UIConfigVisualizerPositionSubscriber> m_positionSubscriber;

	// Add these methods
	void InitializePositionSubscriber();
	void CleanupPositionSubscriber();
	void SubscribeToControllers();

	bool m_debugLoggingEnabled = false;

	void RenderDevicePositionEntry(const std::string& deviceName, const PositionStruct& position);
	void RenderDeviceHeader(const std::string& deviceName, bool isMoving);
	void RenderDevicePositionInfo(const std::string& deviceName);
	void RenderDeviceCoordinates(const std::string& deviceName, const PositionStruct& position);
	void RenderDeviceSaveButton(const std::string& deviceName);
	void RenderPositionUpdateTime(const std::string& deviceName);
	void RenderRealtimeMode();
	void RenderPollingMode();
	void RenderDebugInfo();
	void RenderAutoRefreshControls();
};