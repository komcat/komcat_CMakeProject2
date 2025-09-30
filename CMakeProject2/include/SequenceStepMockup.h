#pragma once
//#include "AppContext.h"
#include "ProcessStep.h"
#include "SequenceStep.h"
#include "PickAndPlaceParam.h"
#include <iostream>
#include <string>


class MachineOperations; // Forward declaration


class DoSomethingA : public SequenceOperation {
public:
	DoSomethingA(std::string task) {
		m_task = task;
	}

	bool Execute(MachineOperations& ops) override {
		ops.LogInfo(" A is doing: " + m_task);
		// Simulate work
		ops.Wait(500);
		return true; // Indicate success
	}

	std::string GetDescription() const override {
		return "Perform " + m_task + "by A";
	}
private:
	std::string m_task;
};


// Updated PickAndPlace class using parameters
class PickAndPlaceMockup : public SequenceOperation {
public:
	// Option 2: Constructor with param object
	PickAndPlaceMockup(const PickAndPlaceParam& param)
		: m_param(param) {
	}


	bool Execute(MachineOperations& ops) override {
		if (!m_param.isEnabled()) {
			ops.LogInfo(m_param.getDeviceName() + " is disabled, skipping operation");
			return true;
		}

		ops.LogInfo(m_param.getDeviceName() + " is moving to: " + m_param.getNodePick() +
			" at speed: " + std::to_string(m_param.getSpeed()));

		// Simulate work based on speed (higher speed = less time)
		int waitTime = static_cast<int>(1000.0 / m_param.getSpeed() * 50); // Scale factor
		ops.Wait(waitTime);

		ops.LogInfo(m_param.getDeviceName() + " is moving to: " + m_param.getNodePlace());
		ops.Wait(waitTime);

		ops.LogInfo(m_param.getDeviceName() + " is stopped at: " + m_param.getNodePlace());
		return true;
	}

	std::string GetDescription() const override {
		// Create a non-const reference to access param methods
		auto& param = const_cast<PickAndPlaceParam&>(m_param);
		return "Pick and Place: " + param.getDeviceName() +
			" from " + param.getNodePick() +
			" to " + param.getNodePlace();
	}

	// Access to param for runtime modifications
	PickAndPlaceParam& getParam() { return m_param; }
	const PickAndPlaceParam& getParam() const { return m_param; }

private:
	PickAndPlaceParam m_param;
};






