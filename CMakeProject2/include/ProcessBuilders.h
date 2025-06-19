#pragma once

#include "SequenceStep.h"
#include "machine_operations.h"
#include <memory>
#include <functional>
#include <string>

// Forward declaration for user interaction
class UserInteractionManager;

// Additional operation types needed for our processes



// User interaction manager (declaration)
class UserInteractionManager {
public:
	UserInteractionManager() = default;
	virtual ~UserInteractionManager() = default;

	// Wait for user confirmation with a message
	virtual bool WaitForConfirmation(const std::string& message) = 0;
};

// User confirmation operation
class UserConfirmOperation : public SequenceOperation {
public:
	UserConfirmOperation(const std::string& message, UserInteractionManager& uiManager)
		: m_message(message), m_uiManager(uiManager) {
	}

	bool Execute(MachineOperations& ops) override {
		return m_uiManager.WaitForConfirmation(m_message);
	}

	std::string GetDescription() const override {
		return "Wait for user confirmation: " + m_message;
	}

private:
	std::string m_message;
	UserInteractionManager& m_uiManager;
};

// Process builder namespace - contains factory functions to build sequence steps for specific processes
namespace ProcessBuilders {

	// Build an initialization sequence Parallel move to point motion
	std::unique_ptr<SequenceStep> BuildInitializationSequenceParallel(MachineOperations& machineOps);


	// Build an initialization sequence
	std::unique_ptr<SequenceStep> BuildInitializationSequence(MachineOperations& machineOps);

	// Build a probing sequence
	std::unique_ptr<SequenceStep> BuildProbingSequence(
		MachineOperations& machineOps, UserInteractionManager& uiManager);

	// Build a pick and place left lens sequence
	std::unique_ptr<SequenceStep> BuildPickPlaceLeftLensSequence(
		MachineOperations& machineOps, UserInteractionManager& uiManager);

	// Build a pick and place right lens sequence
	std::unique_ptr<SequenceStep> BuildPickPlaceRightLensSequence(
		MachineOperations& machineOps, UserInteractionManager& uiManager);

	// Build a UV curing sequence
	std::unique_ptr<SequenceStep> BuildUVCuringSequence(MachineOperations& machineOps, UserInteractionManager& uiManager);

	// Build a complete process sequence (combines all steps)
	std::unique_ptr<SequenceStep> BuildCompleteProcessSequence(
		MachineOperations& machineOps, UserInteractionManager& uiManager);
	std::unique_ptr<SequenceStep> RejectLeftLensSequence(MachineOperations& machineOps, UserInteractionManager& uiManager);
	std::unique_ptr<SequenceStep> RejectRightLensSequence(MachineOperations& machineOps, UserInteractionManager& uiManager);
	// Debug method to print a sequence without executing it
	void DebugPrintSequence(const std::string& name, const std::unique_ptr<SequenceStep>& sequence);

	// Build an enhanced needle XY calibration sequence  
	std::unique_ptr<SequenceStep> BuildNeedleXYCalibrationSequenceEnhanced(MachineOperations& machineOps, UserInteractionManager& uiManager);

	std::unique_ptr<SequenceStep> BuildDispenseCalibrationSequence(MachineOperations& machineOps, UserInteractionManager& uiManager);
	std::unique_ptr<SequenceStep> BuildDispenseCalibration2Sequence(MachineOperations& machineOps, UserInteractionManager& uiManager);
	std::unique_ptr<SequenceStep> BuildDispenseEpoxy1Sequence(MachineOperations& machineOps, UserInteractionManager& uiManager);
	std::unique_ptr<SequenceStep> BuildDispenseEpoxy2Sequence(MachineOperations& machineOps, UserInteractionManager& uiManager);

}