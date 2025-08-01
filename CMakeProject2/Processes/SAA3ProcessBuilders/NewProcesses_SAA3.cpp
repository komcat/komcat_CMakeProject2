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



        //// 5. Clear output R_Gripper (pin 2)
        //sequence->AddOperation(std::make_shared<SetOutputOperation>(
        //    "IOBottom", 2, false));

        // 6. Retract UV_Head pneumatic
        sequence->AddOperation(std::make_shared<RetractSlideOperation>(
            "UV_Head"));

        // 7. Retract Dispenser_Head pneumatic
        sequence->AddOperation(std::make_shared<RetractSlideOperation>(
            "Dispenser_Head"));

        // 8. Retract Pick_Up_Tool pneumatic
        sequence->AddOperation(std::make_shared<RetractSlideOperation>(
            "Pick_Up_Tool"));

        // 2. Move to safe positions (adjust nodes for SAA3)
        // TODO: Replace with actual SAA3 node names
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "gantry-main", "Process_Flow", "node_home"));


        // 3. Move hex-right to home position
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "hex-right", "Process_Flow", "node_8550")); //gripper home

        // on vacuu,
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "IOBottom", 7,true));  // Clear Vacuum_Base (pin 10)
        // open gripper
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "IOBottom", 2, false));  // Clear Vacuum_Base (pin 10)




        return sequence;
    }

    // ========================================================================
    // SAA3 Process 2: PickPlace FAU
    // ========================================================================
    std::unique_ptr<SequenceStep> BuildSAA3PickPlaceFAU(MachineOperations& machineOps, UserPromptUI& promptUI) {
        auto sequence = std::make_unique<SequenceStep>("SAA3 PickPlace FAU", machineOps);

        // TODO: Implement SAA3 pick and place FAU sequence
        // Placeholder implementation - replace with actual SAA3 pick/place steps




        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "gantry-main", "Process_Flow", "node_FAU"));


        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "hex-right", "Process_Flow", "node_8593")); //grip FAU


        // 4. Wait for user confirmation that grip is successful
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Grip Confirmation",
            "Confirm to grip?",
            promptUI));

        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "IOBottom", 2, true));  // Set RIGHT gripper

        sequence->AddOperation(std::make_shared<WaitOperation>(1000));

        // Release and re-grip cycle for the RIGHT lens
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "IOBottom", 2, false));  // Release RIGHT gripper

        sequence->AddOperation(std::make_shared<WaitOperation>(1000));

        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "IOBottom", 2, true));  // Set RIGHT gripper


        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "FAU gripped confirmation",
            "Yes to continue to place.",
            promptUI));


        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "gantry-main", "Process_Flow", "node_PIC"));

        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "hex-right", "Process_Flow", "node_8601"));




        return sequence;
    }

    // ========================================================================
    // SAA3 Process 3: DispenseEpoxy FAU
    // ========================================================================
    std::unique_ptr<SequenceStep> BuildSAA3DispenseEpoxyFAU(MachineOperations& machineOps, UserPromptUI& promptUI) {
        auto sequence = std::make_unique<SequenceStep>("SAA3 DispenseEpoxy FAU", machineOps);

       



        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "gantry-main", "Process_Flow", "node_dispense_FAU"));




        sequence->AddOperation(std::make_shared<ExtendSlideOperation>(
            "Dispenser_Head"));

        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Please manual jog to dispense position",
            "Continue click Yes?",
            promptUI));

        sequence->AddOperation(std::make_shared<MoveRelativeOperation>(
            "hex-right", "Z", -0.1)); // --> to the left

        sequence->AddOperation(std::make_shared<MoveRelativeOperation>(
            "gantry-main", "X", 0.3)); // --> to the left


        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "IOBottom", 15, true,0));  // Start epoxy flow


        sequence->AddOperation(std::make_shared<WaitOperation>(1800));  // Dispense time

        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "IOBottom", 15, false, 0));  // stop epoxy flow

        sequence->AddOperation(std::make_shared<MoveRelativeOperation>(
            "gantry-main", "X", -0.3)); // Move left 0.1mm on Y axis

        sequence->AddOperation(std::make_shared<MoveRelativeOperation>(
            "hex-right", "Z", 0.1)); // Move up 0.1mm on Z axis

        // 7. Stop epoxy dispensing
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "IOBottom", 5, false));  // Stop epoxy flow

        // 8. Retract dispensing head
        sequence->AddOperation(std::make_shared<RetractSlideOperation>(
            "Dispenser_Head"));






        return sequence;
    }

    // ========================================================================
    // SAA3 Process 4: UV Curing
    // ========================================================================
    std::unique_ptr<SequenceStep> BuildSAA3UVCuring(MachineOperations& machineOps, UserPromptUI& promptUI) {
        auto sequence = std::make_unique<SequenceStep>("SAA3 UV Curing", machineOps);


        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "gantry-main", "Process_Flow", "node_UV"));


        sequence->AddOperation(std::make_shared<ExtendSlideOperation>(
            "UV_Head"));

        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Start UV curing Step1 confirmation",
            "Continue click Yes?",
            promptUI));

        //create rising edge IO 14 for UV PLC1
        sequence->AddOperation(std::make_shared<ClearOutputOperation>(
            "IOBottom", 14));  
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "IOBottom", 14, true));  
        sequence->AddOperation(std::make_shared<ClearOutputOperation>(
            "IOBottom", 14));  

        // wait 3 minutes
        sequence->AddOperation(std::make_shared<WaitOperation>(90000));

        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Click Yes when UV step 1 finished to continue to UV step 2",
            "Continue click Yes.",
            promptUI));

        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Click Yes to Continue UV step 2",
            "Continue click Yes?",
            promptUI));

        //create rising edge IO 14 for UV PLC1
        sequence->AddOperation(std::make_shared<ClearOutputOperation>(
            "IOBottom", 13));
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "IOBottom", 13, true));
        sequence->AddOperation(std::make_shared<ClearOutputOperation>(
            "IOBottom", 13));


        sequence->AddOperation(std::make_shared<WaitOperation>(300000));


        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Click Yes when UV Step 2 finished, Yes will lift up UV head",
            "Continue click Yes?",
            promptUI));

        // 10. Retract UV head
        sequence->AddOperation(std::make_shared<RetractSlideOperation>(
            "UV_Head"));

        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "gantry-main", "Process_Flow", "node_home"));

        sequence->AddOperation(std::make_shared<ClearOutputOperation>(
            "IOBottom", 2, 3000)); //clear gripper

        sequence->AddOperation(std::make_shared<MoveToPointNameOperation>(
            "hex-right", "AvoidPlace"));


        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Congratulation, you successfully UV cure.",
            "Continue click Yes?",
            promptUI));

        return sequence;
    }

    // ========================================================================
    // SAA3 Process 5: Reject FAU
    // ========================================================================
    std::unique_ptr<SequenceStep> BuildSAA3RejectFAU(MachineOperations& machineOps, UserPromptUI& promptUI) {
        auto sequence = std::make_unique<SequenceStep>("SAA3 Reject FAU", machineOps);

        sequence->AddOperation(std::make_shared<MoveToPointNameOperation>(
            "hex-right", "AvoidPlace"));

        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "hex-right", "Process_Flow", "node_8593")); //grip FAU

        sequence->AddOperation(std::make_shared<ClearOutputOperation>(
            "IOBottom", 2, 3000)); //clear gripper

        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "hex-right", "Process_Flow", "node_8550")); //gripper home

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
        return 6; // Update this if you add more SAA3 processes
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