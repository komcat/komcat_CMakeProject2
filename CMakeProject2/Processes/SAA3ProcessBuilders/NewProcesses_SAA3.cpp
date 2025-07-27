// ============================================================================
// NewProcesses_SAA3.cpp - SAA3 Machine Process Implementations
// ============================================================================

#include "NewProcesses_SAA3.h"
#include "ProcessRegistry.h"
#include "uaa3_process_builders.h"

namespace SAA3Processes {

    // ========================================================================
    // SAA3 Process 1: Initial
    // ========================================================================
    std::unique_ptr<SequenceStep> BuildSAA3Initial(MachineOperations& machineOps, UserPromptUI& promptUI) {
        auto sequence = std::make_unique<SequenceStep>("SAA3 Initial", machineOps);

        // TODO: Implement SAA3 initialization sequence
        // Placeholder implementation - replace with actual SAA3 initialization steps

        // 1. Welcome prompt
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "SAA3 Initialization",
            "Starting SAA3 machine initialization sequence.\n\n"
            "This will initialize all SAA3 systems and move\n"
            "all devices to their safe starting positions.\n\n"
            "Click YES to begin initialization.",
            promptUI));

        // 2. Move to safe positions (adjust nodes for SAA3)
        // TODO: Replace with actual SAA3 node names
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "saa3-gantry", "SAA3_Process_Flow", "saa3_safe_position"));

        // 3. Initialize outputs (adjust for SAA3 hardware)
        // TODO: Update with actual SAA3 IO configuration
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "SAA3_IO", 0, false));  // Clear gripper output

        // 4. Retract pneumatics (adjust for SAA3 hardware)
        // TODO: Update with actual SAA3 pneumatic names
        sequence->AddOperation(std::make_shared<RetractSlideOperation>(
            "SAA3_Gripper"));

        // 5. Completion confirmation
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "SAA3 Initialization Complete",
            "SAA3 machine initialization completed successfully.\n\n"
            "All systems are now in safe starting positions:\n"
            "• Gantry: Safe position\n"
            "• Pneumatics: Retracted\n"
            "• Outputs: Cleared\n\n"
            "SAA3 is ready for operation.",
            promptUI));

        return sequence;
    }

    // ========================================================================
    // SAA3 Process 2: PickPlace FAU
    // ========================================================================
    std::unique_ptr<SequenceStep> BuildSAA3PickPlaceFAU(MachineOperations& machineOps, UserPromptUI& promptUI) {
        auto sequence = std::make_unique<SequenceStep>("SAA3 PickPlace FAU", machineOps);

        // TODO: Implement SAA3 pick and place FAU sequence
        // Placeholder implementation - replace with actual SAA3 pick/place steps

        // 1. Start prompt
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "SAA3 PickPlace FAU",
            "Starting SAA3 pick and place FAU operation.\n\n"
            "This process will:\n"
            "• Move to FAU pickup position\n"
            "• Grip the FAU component\n"
            "• Move to placement position\n"
            "• Place the FAU component\n\n"
            "Ensure FAU is ready for pickup.",
            promptUI));

        // 2. Move to FAU pickup position
        // TODO: Replace with actual SAA3 FAU pickup node
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "saa3-gantry", "SAA3_Process_Flow", "saa3_fau_pickup"));

        // 3. Position verification
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "FAU Pickup Position",
            "Verify FAU component is correctly positioned for pickup.\n\n"
            "Check that:\n"
            "• FAU is properly aligned\n"
            "• No obstructions present\n"
            "• Gripper is positioned correctly\n\n"
            "Click YES to proceed with pickup.",
            promptUI));

        // 4. Activate gripper
        // TODO: Update with actual SAA3 gripper control
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "SAA3_IO", 0, true));  // Activate FAU gripper

        // 5. Wait for grip stabilization
        sequence->AddOperation(std::make_shared<WaitOperation>(1000));

        // 6. Grip confirmation
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "FAU Grip Confirmation",
            "Confirm FAU component is securely gripped.\n\n"
            "Verify the gripper has properly secured the FAU.\n"
            "Click YES if grip is secure, NO to abort.",
            promptUI));

        // 7. Move to placement position
        // TODO: Replace with actual SAA3 FAU placement node
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "saa3-gantry", "SAA3_Process_Flow", "saa3_fau_placement"));

        // 8. Place FAU
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "SAA3_IO", 0, false));  // Release FAU gripper

        // 9. Placement confirmation
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "FAU Placement Complete",
            "SAA3 FAU pick and place operation completed.\n\n"
            "FAU component has been successfully:\n"
            "• Picked up from source position\n"
            "• Transported safely\n"
            "• Placed at target position\n\n"
            "Ready for next operation.",
            promptUI));

        return sequence;
    }

    // ========================================================================
    // SAA3 Process 3: DispenseEpoxy FAU
    // ========================================================================
    std::unique_ptr<SequenceStep> BuildSAA3DispenseEpoxyFAU(MachineOperations& machineOps, UserPromptUI& promptUI) {
        auto sequence = std::make_unique<SequenceStep>("SAA3 DispenseEpoxy FAU", machineOps);

        // TODO: Implement SAA3 epoxy dispensing for FAU
        // Placeholder implementation - replace with actual SAA3 dispensing steps

        // 1. Start prompt
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "SAA3 DispenseEpoxy FAU",
            "Starting SAA3 epoxy dispensing for FAU.\n\n"
            "This process will:\n"
            "• Move to dispensing position\n"
            "• Prepare dispensing system\n"
            "• Apply precise epoxy pattern\n"
            "• Complete dispensing cycle\n\n"
            "Ensure epoxy system is primed and ready.",
            promptUI));

        // 2. Move to dispensing position
        // TODO: Replace with actual SAA3 dispensing node
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "saa3-gantry", "SAA3_Process_Flow", "saa3_fau_dispense_position"));

        // 3. Activate dispensing head
        // TODO: Update with actual SAA3 dispenser control
        sequence->AddOperation(std::make_shared<ExtendSlideOperation>(
            "SAA3_Dispenser_Head"));

        // 4. Pre-dispense verification
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Pre-Dispense Check",
            "Verify dispensing setup before starting.\n\n"
            "Check that:\n"
            "• Dispenser head is properly positioned\n"
            "• FAU is correctly aligned\n"
            "• Epoxy flow is ready\n\n"
            "Click YES to begin dispensing.",
            promptUI));

        // 5. Start epoxy dispensing
        // TODO: Update with actual SAA3 dispensing control
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "SAA3_IO", 5, true));  // Start epoxy flow

        // 6. Dispensing pattern (simulate with moves and timing)
        // TODO: Replace with actual SAA3 dispensing pattern
        sequence->AddOperation(std::make_shared<WaitOperation>(2000));  // Dispense time

        // 7. Stop epoxy dispensing
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "SAA3_IO", 5, false));  // Stop epoxy flow

        // 8. Retract dispensing head
        sequence->AddOperation(std::make_shared<RetractSlideOperation>(
            "SAA3_Dispenser_Head"));

        // 9. Quality check
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Dispensing Quality Check",
            "Inspect epoxy dispensing quality.\n\n"
            "Verify that:\n"
            "• Epoxy pattern is complete\n"
            "• No air bubbles present\n"
            "• Coverage is adequate\n\n"
            "Click YES if quality is acceptable.",
            promptUI));

        // 10. Completion confirmation
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "SAA3 Epoxy Dispensing Complete",
            "SAA3 epoxy dispensing for FAU completed successfully.\n\n"
            "Dispensing results:\n"
            "• Pattern applied correctly\n"
            "• Quality verified\n"
            "• System ready for curing\n\n"
            "Proceed to UV curing when ready.",
            promptUI));

        return sequence;
    }

    // ========================================================================
    // SAA3 Process 4: UV Curing
    // ========================================================================
    std::unique_ptr<SequenceStep> BuildSAA3UVCuring(MachineOperations& machineOps, UserPromptUI& promptUI) {
        auto sequence = std::make_unique<SequenceStep>("SAA3 UV Curing", machineOps);

        // TODO: Implement SAA3 UV curing sequence
        // Placeholder implementation - replace with actual SAA3 curing steps

        // 1. Start prompt
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "SAA3 UV Curing",
            "Starting SAA3 UV curing process.\n\n"
            "This process will:\n"
            "• Position UV head over component\n"
            "• Set optimal temperature\n"
            "• Apply UV light for curing\n"
            "• Monitor curing progress\n\n"
            "Ensure safety glasses are worn.",
            promptUI));

        // 2. Move to curing position
        // TODO: Replace with actual SAA3 curing node
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "saa3-gantry", "SAA3_Process_Flow", "saa3_uv_curing_position"));

        // 3. Extend UV head
        // TODO: Update with actual SAA3 UV head control
        sequence->AddOperation(std::make_shared<ExtendSlideOperation>(
            "SAA3_UV_Head"));

        // 4. Set temperature
        // TODO: Update with actual SAA3 temperature control
        sequence->AddOperation(std::make_shared<SetTECTemperatureOperation>(30.0f));

        // 5. Wait for temperature stabilization
        sequence->AddOperation(std::make_shared<WaitForLaserTemperatureOperation>(
            30.0f, 2.0f, 10000));

        // 6. Pre-curing safety check
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "UV Curing Safety Check",
            "SAFETY: UV curing about to begin.\n\n"
            "Ensure that:\n"
            "• All personnel have safety glasses\n"
            "• Work area is clear\n"
            "• Component is properly positioned\n\n"
            "Click YES to start UV curing.",
            promptUI));

        // 7. Start UV curing
        // TODO: Update with actual SAA3 UV control
        sequence->AddOperation(std::make_shared<SetLaserCurrentOperation>(0.300f));
        sequence->AddOperation(std::make_shared<LaserOnOperation>());

        // 8. Curing cycle
        sequence->AddOperation(std::make_shared<WaitOperation>(5000));  // 5 second cure time

        // 9. Stop UV curing
        sequence->AddOperation(std::make_shared<LaserOffOperation>());

        // 10. Retract UV head
        sequence->AddOperation(std::make_shared<RetractSlideOperation>(
            "SAA3_UV_Head"));

        // 11. Cool down
        sequence->AddOperation(std::make_shared<TECOffOperation>());

        // 12. Completion confirmation
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "SAA3 UV Curing Complete",
            "SAA3 UV curing process completed successfully.\n\n"
            "Curing results:\n"
            "• UV exposure completed\n"
            "• Component properly cured\n"
            "• Temperature normalized\n\n"
            "Component is ready for final inspection.",
            promptUI));

        return sequence;
    }

    // ========================================================================
    // SAA3 Process 5: Reject FAU
    // ========================================================================
    std::unique_ptr<SequenceStep> BuildSAA3RejectFAU(MachineOperations& machineOps, UserPromptUI& promptUI) {
        auto sequence = std::make_unique<SequenceStep>("SAA3 Reject FAU", machineOps);

        // TODO: Implement SAA3 FAU rejection sequence
        // Placeholder implementation - replace with actual SAA3 rejection steps

        // 1. Start prompt
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "SAA3 Reject FAU",
            "Starting SAA3 FAU rejection sequence.\n\n"
            "This process will safely remove and dispose\n"
            "of a defective or unwanted FAU component.\n\n"
            "This action will:\n"
            "• Move to FAU position\n"
            "• Safely grip the component\n"
            "• Transport to reject bin\n"
            "• Log rejection reason\n\n"
            "Click YES to proceed with rejection.",
            promptUI));

        // 2. Move to FAU position
        // TODO: Replace with actual SAA3 FAU position node
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "saa3-gantry", "SAA3_Process_Flow", "saa3_fau_current_position"));

        // 3. Rejection reason prompt
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Rejection Reason",
            "Specify the reason for FAU rejection:\n\n"
            "Common reasons:\n"
            "• Quality defect detected\n"
            "• Positioning error\n"
            "• Contamination\n"
            "• Process failure\n\n"
            "This will be logged for quality tracking.",
            promptUI));

        // 4. Activate gripper for rejection
        // TODO: Update with actual SAA3 gripper control
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "SAA3_IO", 0, true));  // Activate reject gripper

        // 5. Wait for secure grip
        sequence->AddOperation(std::make_shared<WaitOperation>(1000));

        // 6. Grip verification
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Reject Grip Verification",
            "Verify FAU is securely gripped for rejection.\n\n"
            "Ensure the gripper has properly secured\n"
            "the component for safe transport.\n\n"
            "Click YES if grip is secure.",
            promptUI));

        // 7. Move to reject bin
        // TODO: Replace with actual SAA3 reject bin node
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "saa3-gantry", "SAA3_Process_Flow", "saa3_reject_bin"));

        // 8. Release into reject bin
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "SAA3_IO", 0, false));  // Release gripper

        // 9. Move to safe position
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "saa3-gantry", "SAA3_Process_Flow", "saa3_safe_position"));

        // 10. Completion confirmation
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "SAA3 FAU Rejection Complete",
            "SAA3 FAU rejection completed successfully.\n\n"
            "Rejection summary:\n"
            "• Component safely removed\n"
            "• Disposed in reject bin\n"
            "• Rejection logged\n"
            "• System returned to safe position\n\n"
            "Ready for next operation.",
            promptUI));

        return sequence;
    }

    // ========================================================================
    // Registration Functions
    // ========================================================================
    void RegisterAllSAA3Processes() {
        auto& registry = ProcessRegistry::GetInstance();

        // Register all SAA3 processes
        registry.RegisterProcess(
            "SAA3_Initial",
            "SAA3_Core",
            "SAA3 machine initialization and system setup",
            true,
            BuildSAA3Initial
        );

        registry.RegisterProcess(
            "SAA3_PickPlaceFAU",
            "SAA3_Core",
            "SAA3 pick and place operation for FAU components",
            true,
            BuildSAA3PickPlaceFAU
        );

        registry.RegisterProcess(
            "SAA3_DispenseEpoxyFAU",
            "SAA3_Dispensing",
            "SAA3 precision epoxy dispensing for FAU components",
            true,
            BuildSAA3DispenseEpoxyFAU
        );

        registry.RegisterProcess(
            "SAA3_UVCuring",
            "SAA3_Curing",
            "SAA3 UV curing process with temperature control",
            true,
            BuildSAA3UVCuring
        );

        registry.RegisterProcess(
            "SAA3_RejectFAU",
            "SAA3_Utility",
            "SAA3 safe rejection and disposal of defective FAU components",
            true,
            BuildSAA3RejectFAU
        );

        printf("SAA3 Processes: Successfully registered 5 SAA3 machine processes\n");
    }

    size_t GetSAA3ProcessCount() {
        return 5; // Update this if you add more SAA3 processes
    }

    bool AreSAA3ProcessesRegistered() {
        auto& registry = ProcessRegistry::GetInstance();
        return registry.HasProcess("SAA3_Initial") &&
            registry.HasProcess("SAA3_PickPlaceFAU") &&
            registry.HasProcess("SAA3_DispenseEpoxyFAU") &&
            registry.HasProcess("SAA3_UVCuring") &&
            registry.HasProcess("SAA3_RejectFAU");
    }

    // ========================================================================
    // AUTO-REGISTRATION - Registers all SAA3 processes automatically
    // ========================================================================
    static struct SAA3AutoRegister {
        SAA3AutoRegister() {
            RegisterAllSAA3Processes();
        }
    } g_saa3AutoRegister;

} // namespace SAA3Processes