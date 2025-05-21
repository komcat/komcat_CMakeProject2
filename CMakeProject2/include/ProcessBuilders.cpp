#include "ProcessBuilders.h"

namespace ProcessBuilders {


	std::unique_ptr<SequenceStep> BuildInitializationSequenceParallel(MachineOperations& machineOps) {
		auto sequence = std::make_unique<SequenceStep>("Initialization", machineOps);

		// Define device positions for parallel movement
		std::vector<std::pair<std::string, std::string>> initialPositions = {
				{"gantry-main", "safe"},      // Move gantry to safe position
				{"hex-left", "home"},         // Move left hexapod to home
				{"hex-right", "home"}         // Move right hexapod to home
		};

		// Add parallel movement operation
		sequence->AddOperation(std::make_shared<ParallelDeviceMovementOperation>(
			initialPositions, "Move all devices to initial positions"));

		// Add the remaining operations to be executed sequentially
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 0, false));  // Clear output L_Gripper (pin 0)

		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 2, false));  // Clear output R_Gripper (pin 2)

		sequence->AddOperation(std::make_shared<RetractSlideOperation>(
			"UV_Head"));  // Retract UV_Head pneumatic

		sequence->AddOperation(std::make_shared<RetractSlideOperation>(
			"Dispenser_Head"));  // Retract Dispenser_Head pneumatic

		sequence->AddOperation(std::make_shared<RetractSlideOperation>(
			"Pick_Up_Tool"));  // Retract Pick_Up_Tool pneumatic

		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 10, true));  // Set output Vacuum_Base (pin 10)

		return sequence;
	}

	std::unique_ptr<SequenceStep> BuildInitializationSequence(MachineOperations& machineOps) {
		auto sequence = std::make_unique<SequenceStep>("Initialization", machineOps);

		sequence->AddOperation(std::make_shared<WaitForCameraReadyOperation>(5000));

		// 1. Move gantry-main to safe position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4027"));

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

		// 6. Retract UV_Head pneumatic
		sequence->AddOperation(std::make_shared<RetractSlideOperation>(
			"UV_Head"));

		// 7. Retract Dispenser_Head pneumatic
		sequence->AddOperation(std::make_shared<RetractSlideOperation>(
			"Dispenser_Head"));

		// 8. Retract Pick_Up_Tool pneumatic
		sequence->AddOperation(std::make_shared<RetractSlideOperation>(
			"Pick_Up_Tool"));

		// 9. Set output Vacuum_Base (pin 10)
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 10, true));

		return sequence;
	}

	std::unique_ptr<SequenceStep> BuildProbingSequence(
		MachineOperations& machineOps, UserInteractionManager& uiManager) {

		auto sequence = std::make_unique<SequenceStep>("Probing", machineOps);

		// 1. Move gantry to see sled position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4083"));

		// 2. Wait for user to confirm sled position
		sequence->AddOperation(std::make_shared<UserConfirmOperation>(
			"Please check sled position and confirm to continue", uiManager));


		// 1. Turn on TEC and set temperature
		sequence->AddOperation(std::make_shared<TECOnOperation>());
		sequence->AddOperation(std::make_shared<SetTECTemperatureOperation>(25.0f));

		// 2. Wait for temperature to stabilize
		sequence->AddOperation(std::make_shared<WaitForLaserTemperatureOperation>(
			25.0f, 1.0f, 5000)); //100ms

		// 3. Set laser current and turn on laser
		sequence->AddOperation(std::make_shared<SetLaserCurrentOperation>(0.250f)); // 150mA
		sequence->AddOperation(std::make_shared<LaserOnOperation>());

		// 4. Wait for processing time
		sequence->AddOperation(std::make_shared<WaitOperation>(500)); // 100ms

		// 5. Turn off laser and TEC
		//sequence->AddOperation(std::make_shared<LaserOffOperation>());
		//sequence->AddOperation(std::make_shared<TECOffOperation>());

		// 3. Move gantry to see PIC position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4107"));

		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-right", "Process_Flow", "node_5211"));

		// 4. Wait for user to confirm PIC position
		sequence->AddOperation(std::make_shared<UserConfirmOperation>(
			"Please check PIC position and confirm to continue", uiManager));

		// 5. Move back to safe position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4027"));

		return sequence;
	}

	std::unique_ptr<SequenceStep> BuildPickPlaceLeftLensSequence(
		MachineOperations& machineOps, UserInteractionManager& uiManager) {

		auto sequence = std::make_unique<SequenceStep>("Pick and Place Left Lens", machineOps);



		// 2. Move hex-left to pick lens position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-left", "Process_Flow", "node_5647"));


		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4186"));	//see pick collimate lens


		// 4. Wait for user confirmation that grip is successful
		sequence->AddOperation(std::make_shared<UserConfirmOperation>(
			"Confirm left lens is successfully gripped", uiManager));



		// 3. Set output L-gripper (pin 0) to grab the lens
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 0, true));

		// 4. Start camera grabbing
		sequence->AddOperation(std::make_shared<StartCameraGrabbingOperation>());

		// 5. Wait for camera to stabilize
		sequence->AddOperation(std::make_shared<WaitOperation>(500));  // 500ms delay

		// 6. Capture image
		sequence->AddOperation(std::make_shared<CaptureImageOperation>());




		// 5. Release the lens temporarily (clear output)
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 0, false));

		// 6. Wait 1.5 seconds
		sequence->AddOperation(std::make_shared<WaitOperation>(1500));

		// 7. Grip the lens again (set output)
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 0, true, 500));

		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4137"));	//see collimate lens



		// 10. Move to placement position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-left", "Process_Flow", "node_5662"));

		// 4. Start camera grabbing
		sequence->AddOperation(std::make_shared<WaitForCameraReadyOperation>());
		sequence->AddOperation(std::make_shared<StartCameraGrabbingOperation>());
		sequence->AddOperation(std::make_shared<CaptureImageOperation>("place_check.png"));


		return sequence;
	}

	std::unique_ptr<SequenceStep> BuildPickPlaceRightLensSequence(
		MachineOperations& machineOps, UserInteractionManager& uiManager) {

		auto sequence = std::make_unique<SequenceStep>("Pick and Place Right Lens", machineOps);

		// Read and log initial laser current and temperature
		sequence->AddOperation(std::make_shared<ReadAndLogLaserCurrentOperation>(
			"", "Initial laser current"));
		sequence->AddOperation(std::make_shared<ReadAndLogLaserTemperatureOperation>(
			"", "Initial laser temperature"));
		sequence->AddOperation(std::make_shared<ReadAndLogDataValueOperation>(
			"GPIB-Current", "(GPIB-Current) Dry (only collimate) reading"));


		// Move hex-right to pick lens position (verify this is correct for RIGHT lens)
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-right", "Process_Flow", "node_5245"));

		// Move gantry to see the RIGHT lens pick position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4209"));  // Verify this is correct for focus lens


		// Wait for user confirmation
		sequence->AddOperation(std::make_shared<UserConfirmOperation>(
			"Confirm right lens is successfully gripped", uiManager));

		// Use R-gripper (pin 2) for RIGHT lens
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 2, true));  // Set RIGHT gripper

		// 4. Start camera grabbing
		sequence->AddOperation(std::make_shared<StartCameraGrabbingOperation>());

		// 5. Wait for camera to stabilize
		sequence->AddOperation(std::make_shared<WaitOperation>(500));  // 500ms delay

		// 6. Capture image
		sequence->AddOperation(std::make_shared<CaptureImageOperation>());

		// Release and re-grip cycle for the RIGHT lens
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 2, false));  // Release RIGHT gripper
		sequence->AddOperation(std::make_shared<WaitOperation>(1500));



		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 2, true, 500));  // Re-grip RIGHT lens

		// Move gantry to see the RIGHT lens
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4156"));  // Verify this is for focus lens

		// Move to RIGHT lens placement position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-right", "Process_Flow", "node_5263"));  // Verify this is correct for RIGHT placement

		return sequence;
	}

	std::unique_ptr<SequenceStep> BuildUVCuringSequence(MachineOperations& machineOps,
		UserInteractionManager& uiManager) {
		auto sequence = std::make_unique<SequenceStep>("UV Curing", machineOps);


		// 1. Move gantry to UV position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4426"));

		sequence->AddOperation(std::make_shared<SetLaserCurrentOperation>(0.150f)); // 150mA



		// 2. Extend UV_Head pneumatic
		sequence->AddOperation(std::make_shared<ExtendSlideOperation>("UV_Head"));
		// Read and log initial laser current and temperature
		sequence->AddOperation(std::make_shared<ReadAndLogLaserCurrentOperation>(
			"", "Read laser current"));
		sequence->AddOperation(std::make_shared<ReadAndLogLaserTemperatureOperation>(
			"", "Read laser temperature"));
		sequence->AddOperation(std::make_shared<ReadAndLogDataValueOperation>(
			"GPIB-Current", "(GPIB-Current) Dry Alignment (before fine tune)"));

		// 4. Wait for user to do fine alignment
		sequence->AddOperation(std::make_shared<UserConfirmOperation>(
			"Confirm to fine align lens again (um steps =0.5, 0.2, 0.1) ", uiManager));

		// 2. Perform scan (will automatically move to peak)
		sequence->AddOperation(std::make_shared<RunScanOperation>(
			"hex-left", "GPIB-Current",
			std::vector<double>{ 0.0002, 0.0001},
			300, // settling time in ms
			std::vector<std::string>{"Z", "X", "Y"}));

		// 2. Perform scan (will automatically move to peak)
		sequence->AddOperation(std::make_shared<RunScanOperation>(
			"hex-right", "GPIB-Current",
			std::vector<double>{0.0002, 0.0001},
			300, // settling time in ms
			std::vector<std::string>{"Z", "X", "Y"}));

		// Read and log initial laser current and temperature
		sequence->AddOperation(std::make_shared<ReadAndLogLaserCurrentOperation>(
			"", "Read laser current"));
		sequence->AddOperation(std::make_shared<ReadAndLogLaserTemperatureOperation>(
			"", "Read laser temperature"));
		sequence->AddOperation(std::make_shared<ReadAndLogDataValueOperation>(
			"GPIB-Current", "(GPIB-Current) Dry Alignment (Before UV)"));


		// 4. Wait for user confirmation that grip is successful
		sequence->AddOperation(std::make_shared<UserConfirmOperation>(
			"Confirm start UV curing (take 210 seconds)", uiManager));


		// 3. Toggle UV_PLC1 (pin 14) - Clear output
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 14, false));

		// 4. Wait 50ms
		sequence->AddOperation(std::make_shared<WaitOperation>(50));

		// 5. Toggle UV_PLC1 (pin 14) - Set output
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 14, true));

		// 6. Wait 150ms
		sequence->AddOperation(std::make_shared<WaitOperation>(150));

		// 7. Wait for curing (200 seconds)
		//sequence->AddOperation(std::make_shared<WaitOperation>(210000));
		sequence->AddOperation(std::make_shared<PeriodicMonitorDataValueOperation>(
			"GPIB-Current", 210000, 5000)); // Monitor every 5 seconds for 210 seconds total



		// 8. Retract UV_Head
		sequence->AddOperation(std::make_shared<RetractSlideOperation>("UV_Head"));

		// 9. Clear left and right grippers
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 0, false)); // Left gripper

		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 2, false)); // Right gripper

		// 6. Wait 150ms
		sequence->AddOperation(std::make_shared<WaitOperation>(1500));

		// 10. Move hex-left to approach position
		sequence->AddOperation(std::make_shared<MoveToPointNameOperation>(
			"hex-left", "approachlensplace"));

		// 11. Move hex-right to approach position
		sequence->AddOperation(std::make_shared<MoveToPointNameOperation>(
			"hex-right", "approachlensplace"));

		// 12. Move hex-left to home
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-left", "Process_Flow", "node_5480"));

		// 13. Move hex-right to home
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-right", "Process_Flow", "node_5136"));

		// 14. Move gantry to safe position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4027"));


		// Read and log initial laser current and temperature
		sequence->AddOperation(std::make_shared<ReadAndLogLaserCurrentOperation>(
			"", "Read laser current"));
		sequence->AddOperation(std::make_shared<ReadAndLogLaserTemperatureOperation>(
			"", "Read laser temperature"));
		sequence->AddOperation(std::make_shared<ReadAndLogDataValueOperation>(
			"GPIB-Current", "(GPIB-Current) After UV reading"));


		// 5. Turn off laser and TEC
		sequence->AddOperation(std::make_shared<LaserOffOperation>());
		sequence->AddOperation(std::make_shared<TECOffOperation>());


		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 10, false));  // clear output Vacuum_Base (pin 10)

		return sequence;
	}


	//complete process for automation
	std::unique_ptr<SequenceStep> BuildCompleteProcessSequence(
		MachineOperations& machineOps, UserInteractionManager& uiManager) {

		auto sequence = std::make_unique<SequenceStep>("Complete Process", machineOps);

		// Add all sub-sequences as steps in the complete process

		// 1. Initialization
		auto initSequence = BuildInitializationSequence(machineOps);
		for (const auto& op : initSequence->GetOperations()) {
			sequence->AddOperation(op);
		}

		// 2. Probing
		auto probingSequence = BuildProbingSequence(machineOps, uiManager);
		for (const auto& op : probingSequence->GetOperations()) {
			sequence->AddOperation(op);
		}

		// 3. Pick and Place Left Lens
		auto leftLensSequence = BuildPickPlaceLeftLensSequence(machineOps, uiManager);
		for (const auto& op : leftLensSequence->GetOperations()) {
			sequence->AddOperation(op);
		}

		// 4. Pick and Place Right Lens
		auto rightLensSequence = BuildPickPlaceRightLensSequence(machineOps, uiManager);
		for (const auto& op : rightLensSequence->GetOperations()) {
			sequence->AddOperation(op);
		}

		// 5. UV Curing
		auto uvCuringSequence = BuildUVCuringSequence(machineOps, uiManager);
		for (const auto& op : uvCuringSequence->GetOperations()) {
			sequence->AddOperation(op);
		}

		return sequence;
	}

	std::unique_ptr<SequenceStep> RejectLeftLensSequence(MachineOperations& machineOps,
		UserInteractionManager& uiManager) {
		auto sequence = std::make_unique<SequenceStep>("Reject Left Lens Process", machineOps);

		// 1. Retract all pneumatics
		sequence->AddOperation(std::make_shared<RetractSlideOperation>("UV_Head"));
		sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));
		sequence->AddOperation(std::make_shared<RetractSlideOperation>("Pick_Up_Tool"));

		// 2. Move gantry to safe position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4027")); // Safe position

		// 3. Move hex-left to reject lens position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-left", "Process_Flow", "node_5531")); // Reject lens position

		// 4. Release left gripper (pin 0)
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 0, false)); // Clear output L_Gripper (pin 0)

		// 5. Wait for 3 seconds to ensure lens is dropped
		sequence->AddOperation(std::make_shared<WaitOperation>(3000)); // 3 seconds

		// 6. Move hex-left back to home position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-left", "Process_Flow", "node_5480")); // Home position

		return sequence;
	}

	std::unique_ptr<SequenceStep> RejectRightLensSequence(MachineOperations& machineOps, UserInteractionManager& uiManager) {
		auto sequence = std::make_unique<SequenceStep>("Reject Right Lens Process", machineOps);

		// 1. Retract all pneumatics
		sequence->AddOperation(std::make_shared<RetractSlideOperation>("UV_Head"));
		sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));
		sequence->AddOperation(std::make_shared<RetractSlideOperation>("Pick_Up_Tool"));

		// 2. Move gantry to safe position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4027")); // Safe position

		// 3. Move hex-right to reject lens position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-right", "Process_Flow", "node_5190")); // Reject lens position

		// 4. Release right gripper (pin 2)
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 2, false)); // Clear output R_Gripper (pin 2)

		// 5. Wait for 3 seconds to ensure lens is dropped
		sequence->AddOperation(std::make_shared<WaitOperation>(3000)); // 3 seconds

		// 6. Move hex-right back to home position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-right", "Process_Flow", "node_5136")); // Home position

		return sequence;
	}



	// Implementation in ProcessBuilders.cpp
	void ProcessBuilders::DebugPrintSequence(const std::string& name, const std::unique_ptr<SequenceStep>& sequence) {
		Logger* logger = Logger::GetInstance();

		logger->LogProcess("=== DEBUG SEQUENCE: " + name + " ===");
		logger->LogProcess("Operation count: " + std::to_string(sequence->GetOperations().size()));

		int i = 1;
		for (const auto& op : sequence->GetOperations()) {
			logger->LogProcess(std::to_string(i++) + ": " + op->GetDescription());
		}

		logger->LogProcess("=== END DEBUG SEQUENCE ===");
	}
} // namespace ProcessBuilders