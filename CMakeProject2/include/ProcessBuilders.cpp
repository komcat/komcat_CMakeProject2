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
		sequence->AddOperation(std::make_shared<WaitOperation>(5000)); // 100ms

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

		// 3. Set output L-gripper (pin 0) to grab the lens
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 0, true));

		// 4. Start camera grabbing
		sequence->AddOperation(std::make_shared<StartCameraGrabbingOperation>());

		// 5. Wait for camera to stabilize
		sequence->AddOperation(std::make_shared<WaitOperation>(500));  // 500ms delay

		// 6. Capture image
		sequence->AddOperation(std::make_shared<CaptureImageOperation>());


		// 4. Wait for user confirmation that grip is successful
		sequence->AddOperation(std::make_shared<UserConfirmOperation>(
			"Confirm left lens is successfully gripped", uiManager));

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



		// 2. Move hex-right to pick lens position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-right", "Process_Flow", "node_5245"));


		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4209"));	//see pick focus lens

		// 3. Set output R-gripper (pin 2) to grab the lens
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 2, true));

		// 4. Wait for user confirmation that grip is successful
		sequence->AddOperation(std::make_shared<UserConfirmOperation>(
			"Confirm right lens is successfully gripped", uiManager));

		// 5. Release the lens temporarily (clear output)
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 2, false));

		// 6. Wait 1.5 seconds
		sequence->AddOperation(std::make_shared<WaitOperation>(1500));

		// 7. Grip the lens again (set output)
		sequence->AddOperation(std::make_shared<SetOutputOperation>(
			"IOBottom", 2, true, 500));

		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"gantry-main", "Process_Flow", "node_4156"));	//see focus lens



		// 10. Move to placement position
		sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
			"hex-right", "Process_Flow", "node_5263"));

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


		// 4. Wait for user confirmation that grip is successful
		sequence->AddOperation(std::make_shared<UserConfirmOperation>(
			"Confirm to fine align lens again (um steps =0.5, 0.2, 0.1) ", uiManager));

		// 2. Perform scan (will automatically move to peak)
		sequence->AddOperation(std::make_shared<RunScanOperation>(
			"hex-left", "GPIB-Current",
			std::vector<double>{0.0005, 0.0002, 0.0001},
			300, // settling time in ms
			std::vector<std::string>{"Z", "X", "Y"}));

		// 2. Perform scan (will automatically move to peak)
		sequence->AddOperation(std::make_shared<RunScanOperation>(
			"hex-right", "GPIB-Current",
			std::vector<double>{0.0005, 0.0002, 0.0001},
			300, // settling time in ms
			std::vector<std::string>{"Z", "X", "Y"}));

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
		sequence->AddOperation(std::make_shared<WaitOperation>(210000));

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

		// 5. Turn off laser and TEC
		sequence->AddOperation(std::make_shared<LaserOffOperation>());
		sequence->AddOperation(std::make_shared<TECOffOperation>());

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

} // namespace ProcessBuilders