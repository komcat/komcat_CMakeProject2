// TCPDataManagerUI.h
#pragma once

#include "include/data/data_client_manager.h"
#include <memory>

class TCPDataManagerUI {
private:
	DataClientManager* m_dataClientManager = nullptr; // Changed to raw pointer
	bool m_isInitialized = false;

public:
	TCPDataManagerUI();
	~TCPDataManagerUI();

	// Initialize with external data client manager
	bool Initialize(DataClientManager* dataClientManager);

	// Update the manager (call this every frame)
	void Update();

	// Render the UI
	void Render();

	// Check if initialized
	bool IsInitialized() const { return m_isInitialized; }

	// Get access to the underlying manager (if needed)
	DataClientManager* GetManager() { return m_dataClientManager; }
};