// ============================================================================
// NewProcesses_Template.cpp - Template for adding new processes
// ============================================================================

#include "ProcessRegistry.h"
#include "uaa3_process_builders.h"

namespace NewProcesses_YourCategory {

    // ========================================================================
    // Example Process 1: Advanced Quality Check
    // ========================================================================
    std::unique_ptr<SequenceStep> BuildAdvancedQualityCheck(MachineOperations& machineOps, UserPromptUI& promptUI) {
        auto sequence = std::make_unique<SequenceStep>("Advanced Quality Check", machineOps);

        // 1. Move to inspection position
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "gantry-main", "Process_Flow", "node_4027"));

        // 2. User prompt for setup verification
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Quality Check Setup",
            "Verify inspection setup is ready.\n\n"
            "Check:\n"
            "• Lighting is optimal\n"
            "• Camera is focused\n"
            "• Test sample is in position\n\n"
            "Click YES to continue with quality inspection.",
            promptUI));

        // 3. Activate inspection systems
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "IOBottom", 15, true));  // Turn on inspection lights

        // 4. Wait for stabilization
        sequence->AddOperation(std::make_shared<WaitOperation>(1000));

        // 5. Multiple inspection points
        std::vector<std::string> inspectionNodes = {
            "node_4083", "node_4107", "node_4137", "node_4186"
        };

        for (size_t i = 0; i < inspectionNodes.size(); ++i) {
            // Move to inspection point
            sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
                "gantry-main", "Process_Flow", inspectionNodes[i]));

            // Inspection prompt
            sequence->AddOperation(UserPromptOperation::CreateBasic(
                "Inspection Point " + std::to_string(i + 1),
                "Inspect quality at point " + std::to_string(i + 1) + "/4\n\n"
                "Verify all parameters are within specification.\n"
                "Click YES if quality is acceptable, NO to abort.",
                promptUI));
        }

        // 6. Final completion prompt
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Quality Check Complete",
            "Advanced quality inspection completed successfully.\n\n"
            "All inspection points have been verified.\n"
            "System ready for next operation.",
            promptUI));

        // 7. Return to safe position
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "gantry-main", "Process_Flow", "node_4027"));

        // 8. Turn off inspection lights
        sequence->AddOperation(std::make_shared<SetOutputOperation>(
            "IOBottom", 15, false));

        return sequence;
    }

    // ========================================================================
    // Example Process 2: Multi-Point Calibration
    // ========================================================================
    std::unique_ptr<SequenceStep> BuildMultiPointCalibration(MachineOperations& machineOps, UserPromptUI& promptUI) {
        auto sequence = std::make_unique<SequenceStep>("Multi-Point Calibration", machineOps);

        // Initialization
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Multi-Point Calibration",
            "Starting comprehensive multi-point calibration.\n\n"
            "This process will calibrate multiple reference points\n"
            "to ensure maximum accuracy across the work area.\n\n"
            "Estimated time: 5-8 minutes",
            promptUI));

        // Calibration points array
        struct CalibrationPoint {
            std::string name;
            std::string node;
            std::string description;
        };

        std::vector<CalibrationPoint> calibrationPoints = {
            {"Front Left", "node_4083", "Front left corner reference"},
            {"Front Right", "node_4107", "Front right corner reference"},
            {"Center", "node_4137", "Center reference point"},
            {"Back Left", "node_4186", "Back left corner reference"},
            {"Back Right", "node_5211", "Back right corner reference"}
        };

        for (const auto& point : calibrationPoints) {
            // Move to calibration point
            sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
                "gantry-main", "Process_Flow", point.node));

            // Wait for stabilization
            sequence->AddOperation(std::make_shared<WaitOperation>(500));

            // User verification
            sequence->AddOperation(UserPromptOperation::CreateBasic(
                "Calibrating: " + point.name,
                "Calibrating " + point.description + "\n\n"
                "Verify position is accurate and stable.\n"
                "Click YES to confirm calibration point.",
                promptUI));

            // Simulate calibration measurement
            sequence->AddOperation(std::make_shared<WaitOperation>(1000));
        }

        // Final verification
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Calibration Complete",
            "Multi-point calibration completed successfully.\n\n"
            "All reference points have been calibrated.\n"
            "System accuracy is now optimized.",
            promptUI));

        return sequence;
    }

    // ========================================================================
    // Example Process 3: Maintenance Routine
    // ========================================================================
    std::unique_ptr<SequenceStep> BuildMaintenanceRoutine(MachineOperations& machineOps, UserPromptUI& promptUI) {
        auto sequence = std::make_unique<SequenceStep>("Maintenance Routine", machineOps);

        // Start maintenance
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Maintenance Routine",
            "Starting automated maintenance routine.\n\n"
            "This will perform:\n"
            "• System diagnostics\n"
            "• Position verification\n"
            "• Hardware checks\n"
            "• Cleanup procedures\n\n"
            "Estimated time: 3-5 minutes",
            promptUI));

        // Home all axes
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "gantry-main", "Process_Flow", "node_4027"));
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "hex-left", "Process_Flow", "node_5480"));
        sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
            "hex-right", "Process_Flow", "node_5136"));

        // System checks
        sequence->AddOperation(std::make_shared<WaitOperation>(2000)); // Simulate diagnostics

        // Clear all outputs for maintenance
        for (int pin = 0; pin <= 15; ++pin) {
            sequence->AddOperation(std::make_shared<SetOutputOperation>(
                "IOBottom", pin, false));
        }

        // Retract all pneumatics
        sequence->AddOperation(std::make_shared<RetractSlideOperation>("UV_Head"));
        sequence->AddOperation(std::make_shared<RetractSlideOperation>("Dispenser_Head"));
        sequence->AddOperation(std::make_shared<RetractSlideOperation>("Pick_Up_Tool"));

        // Final status
        sequence->AddOperation(UserPromptOperation::CreateBasic(
            "Maintenance Complete",
            "Maintenance routine completed successfully.\n\n"
            "System status:\n"
            "• All axes homed\n"
            "• All outputs cleared\n"
            "• All pneumatics retracted\n"
            "• System ready for operation",
            promptUI));

        return sequence;
    }

    // ========================================================================
    // AUTO-REGISTRATION - This runs automatically when the file is included
    // ========================================================================
    static struct AutoRegister {
        AutoRegister() {
            auto& registry = ProcessRegistry::GetInstance();

            // Register all processes defined in this file
            registry.RegisterProcess(
                "UAA3_AdvancedQualityCheck",
                "Quality",
                "Advanced multi-point quality inspection with verification",
                true,
                BuildAdvancedQualityCheck
            );

            registry.RegisterProcess(
                "UAA3_MultiPointCalibration",
                "Calibration",
                "Comprehensive multi-point calibration for maximum accuracy",
                true,
                BuildMultiPointCalibration
            );

            registry.RegisterProcess(
                "UAA3_MaintenanceRoutine",
                "Maintenance",
                "Automated maintenance routine with system diagnostics",
                true,
                BuildMaintenanceRoutine
            );

            // Add more processes here as needed...

            printf("NewProcesses: Registered 3 additional processes\n");
        }
    } g_autoRegister;

} // namespace NewProcesses_YourCategory

// ============================================================================
// USAGE INSTRUCTIONS:
// ============================================================================
/*

To add your 100+ new processes:

1. COPY this file and rename it (e.g., "NewProcesses_Manufacturing.cpp")

2. CHANGE the namespace name to something descriptive:
   namespace NewProcesses_Manufacturing {

3. IMPLEMENT your processes using the template above:
   - Copy the BuildAdvancedQualityCheck function
   - Rename it to your process name
   - Modify the sequence steps for your specific process
   - Update the description

4. UPDATE the AutoRegister section:
   - Add registry.RegisterProcess() calls for each of your new processes
   - Use descriptive categories like "Manufacturing", "Testing", "Quality", etc.

5. INCLUDE the file in your project:
   - Add #include "NewProcesses_Manufacturing.cpp" to your main file
   - OR add the .cpp file to your CMakeLists.txt or project files

6. DONE! Your processes will automatically appear in the UI with:
   - Buttons created automatically
   - Filter support working
   - Category organization
   - Tooltips with descriptions

EXAMPLE: Adding 10 new manufacturing processes:

namespace NewProcesses_Manufacturing {

    std::unique_ptr<SequenceStep> BuildProcess1(...) { ... }
    std::unique_ptr<SequenceStep> BuildProcess2(...) { ... }
    // ... 8 more processes

    static struct AutoRegister {
        AutoRegister() {
            auto& registry = ProcessRegistry::GetInstance();
            registry.RegisterProcess("UAA3_ManufacturingProcess1", "Manufacturing", "...", true, BuildProcess1);
            registry.RegisterProcess("UAA3_ManufacturingProcess2", "Manufacturing", "...", true, BuildProcess2);
            // ... 8 more registrations
        }
    } g_autoRegister;
}

NO OTHER FILES NEED TO BE MODIFIED!

*/