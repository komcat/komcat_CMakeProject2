// Core/uaa3_uv_curing_sequence.cpp
#include "../uaa3_process_builders.h"
#include "UserInputOperations.h"

namespace UAA3ProcessBuilders {

  std::unique_ptr<SequenceStep> BuildUVCuringSequence_uaa3(MachineOperations& machineOps,
    UserPromptUI& promptUI) {
    auto sequence = std::make_unique<SequenceStep>("UV Curing", machineOps);

    // 1. Move gantry to UV position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4426"));

    sequence->AddOperation(std::make_shared<WaitOperation>(300));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("GPIB-Current", "high current @250mA"));


    sequence->AddOperation(std::make_shared<SetLaserCurrentOperation>(0.150f)); // 150mA

    sequence->AddOperation(std::make_shared<WaitOperation>(300));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("GPIB-Current", "low correction @150mA"));

    // 2. Extend UV_Head pneumatic
    sequence->AddOperation(std::make_shared<ExtendSlideOperation>("UV_Head"));
    // Read and log initial laser current and temperature
    sequence->AddOperation(std::make_shared<ReadAndLogLaserCurrentOperation>(
      "", "Read laser current"));
    sequence->AddOperation(std::make_shared<ReadAndLogLaserTemperatureOperation>(
      "", "Read laser temperature"));
    sequence->AddOperation(std::make_shared<ReadAndLogDataValueOperation>(
      "GPIB-Current", "(GPIB-Current) Dry Alignment (before fine tune)"));

    sequence->AddOperation(std::make_shared<WaitOperation>(300));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("GPIB-Current", "before fine correction @150mA"));

    // 4. Wait for user to do fine alignment
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Fine Alignment",
      "Confirm to fine align lens again (um steps =0.5, 0.2, 0.1)",
      promptUI));

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
    sequence->AddOperation(std::make_shared<WaitOperation>(300));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("GPIB-Current", "before UV @150mA"));



    // 4. Wait for user confirmation that grip is successful
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "UV Curing Start",
      "Confirm start UV curing (take 210 seconds)",
      promptUI));

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

    // 6. Wait 150ms
    sequence->AddOperation(std::make_shared<WaitOperation>(1500));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("GPIB-Current", "after UV @150mA"));
    // 8. Retract UV_Head
    sequence->AddOperation(std::make_shared<RetractSlideOperation>("UV_Head"));

    // 9. Clear left and right grippers
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, false)); // Left gripper

    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, false)); // Right gripper



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


    sequence->AddOperation(std::make_shared<WaitOperation>(300));
    sequence->AddOperation(std::make_shared<DUTRecordDataOperation>("GPIB-Current", "after unload @150mA"));
    sequence->AddOperation(std::make_shared<WaitOperation>(1500));
    //CRITICAL END DUT RECORDING AND EXPORT DATA
    sequence->AddOperation(std::make_shared<DUTEndRecordingOperation>(true, true));
    // Clear the stored input at the end
    sequence->AddOperation(std::make_shared<ClearUserInputOperation>("dut_serial"));

    // 5. Turn off laser and TEC
    sequence->AddOperation(std::make_shared<LaserOffOperation>());
    sequence->AddOperation(std::make_shared<TECOffOperation>());

    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 10, false));  // clear output Vacuum_Base (pin 10)

    return sequence;
  }

}