// Core/uaa3_pick_place_sequences.cpp
#include "../uaa3_process_builders.h"
#include <iostream>

namespace UAA3ProcessBuilders {

	std::unique_ptr<SequenceStep> BuildInitializationSequence_uaa3(
		MachineOperations& machineOps, UserPromptUI& promptUI) {

		auto sequence = std::make_unique<SequenceStep>("UAA3 Initialization", machineOps);


		//CRITICAL END DUT RECORDING AND EXPORT DATA
		sequence->AddOperation(std::make_shared<DUTEndRecordingOperation>(true, true));


		sequence->AddOperation(std::make_shared<RetractSlideOperation>(
			"UV_Head"));

		// 7. Retract Dispenser_Head pneumatic
		sequence->AddOperation(std::make_shared<RetractSlideOperation>(
			"Dispenser_Head"));

		// 8. Retract Pick_Up_Tool pneumatic
		sequence->AddOperation(std::make_shared<RetractSlideOperation>(
			"Pick_Up_Tool"));

		// Debug BEFORE adding fallback operation
		std::cout << "Operations before fallback: " << sequence->GetOperations().size() << std::endl;

		// Add with fallback
		sequence->AddOperationWithFallback(
			std::make_shared<MoveToNodeOperation>("gantry-main", "Process_Flow", "node_4027"),
			std::make_shared<MoveToPointNameOperation>("gantry-main", "safe")
		);

		// Debug AFTER adding fallback operation  
		std::cout << "Operations after fallback: " << sequence->GetOperations().size() << std::endl;


		// 2. Move hex-left to home position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-left", "Process_Flow", "node_5480"));

		// 3. Move hex-right to home position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-right", "Process_Flow", "node_5136"));

		// 4. Clear output L_Gripper (pin 0)
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 0, false));

		// 5. Clear output R_Gripper (pin 2)
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 2, false));



		// 9. Clear dedicated output (pin 10) - Vacuum_Base
		sequence->AddOperation(std::make_shared<ClearOutputOperationDedicated>(
			"IOBottom", 10));  // Clear Vacuum_Base (pin 10)





		return sequence;
	}


	std::unique_ptr<SequenceStep> BuildSystemTestSequence(
		MachineOperations& machineOps, UserPromptUI& promptUI) {

		auto sequence = std::make_unique<SequenceStep>("Coordinate System Test", machineOps);

		// 1. Load the Siphog coordinate system from JSON
		sequence->AddOperation(std::make_shared<LoadCoordinateSystemOperation>(
			"Siphog_SYSTEM",  // System name to use in memory
			"system_json/alignment_Siphog_SYSTEM_20250909_195554.json"  // Path to JSON file
		));

		// 2. Move to origin (0,0,0) in Siphog coordinates
		sequence->AddOperation(std::make_shared<MoveToSystemPositionOperation>(
			"gantry-main",     // Device to move
			0.0, 0.0, 0.0,     // Target position in system coordinates
			"Siphog_SYSTEM",   // System name
			true               // Wait for completion
		));

		// 3. Wait 5 seconds
		sequence->AddOperation(std::make_shared<WaitOperation>(5000));  // 5000ms = 5 seconds

		// 4. Move to (-4, 0.62, 0) in Siphog coordinates
		sequence->AddOperation(std::make_shared<MoveToSystemPositionOperation>(
			"gantry-main",      // Device to move
			-4.0, 0.62, 0.0,    // Target position in system coordinates
			"Siphog_SYSTEM",    // System name
			true                // Wait for completion
		));

		// Optional: Display current position in system coordinates
		sequence->AddOperation(std::make_shared<GetPositionInSystemOperation>(
			"gantry-main",
			"Siphog_SYSTEM"
		));

		return sequence;
	}

	// Alternative version with more features for testing
	std::unique_ptr<SequenceStep> BuildExtendedSystemTestSequence(
		MachineOperations& machineOps, UserPromptUI& promptUI) {

		auto sequence = std::make_unique<SequenceStep>("Extended System Test", machineOps);

		// 1. Load the system
		sequence->AddOperation(std::make_shared<LoadCoordinateSystemOperation>(
			"Siphog_SYSTEM",
			"system_json/alignment_Siphog_SYSTEM_20250909_195554.json"
		));

		// 2. Move to origin
		sequence->AddOperation(std::make_shared<MoveToSystemOriginOperation>(
			"gantry-main", "Siphog_SYSTEM", true
		));

		// 3. Wait
		sequence->AddOperation(std::make_shared<WaitOperation>(5000));

		// 4. Move to test position
		sequence->AddOperation(std::make_shared<MoveToSystemPositionOperation>(
			"gantry-main", -4.0, 0.62, 0.0, "Siphog_SYSTEM", true
		));

		// 5. Wait
		sequence->AddOperation(std::make_shared<WaitOperation>(2000));

		// 6. Test relative movements along system axes
		sequence->AddOperation(std::make_shared<MoveRelativeOnSystemOperation>(
			"gantry-main", 2.0, 'X', "Siphog_SYSTEM", true  // Move +2mm along system X
		));

		sequence->AddOperation(std::make_shared<WaitOperation>(2000));

		sequence->AddOperation(std::make_shared<MoveRelativeOnSystemOperation>(
			"gantry-main", -1.0, 'Y', "Siphog_SYSTEM", true  // Move -1mm along system Y
		));

		// 7. Return to origin
		sequence->AddOperation(std::make_shared<MoveToSystemOriginOperation>(
			"gantry-main", "Siphog_SYSTEM", true
		));

		return sequence;
	}
}

