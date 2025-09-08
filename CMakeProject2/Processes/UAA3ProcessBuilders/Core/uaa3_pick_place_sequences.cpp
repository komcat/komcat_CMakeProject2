// Core/uaa3_pick_place_sequences.cpp
#include "../uaa3_process_builders.h"
#include "UserInputOperations.h"
namespace UAA3ProcessBuilders {

  std::unique_ptr<SequenceStep> BuildPickPlaceLeftLensSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Pick and Place Left Lens", machineOps);

    // 2. Move hex-left to pick lens position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5647"));

    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4186"));	//see pick collimate lens

    // 4. Wait for user confirmation that grip is successful
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Lens Position Check",
      "Check the lens position before gripping & click confirm",
      promptUI));

    // 3. Set output L-gripper (pin 0) to grab the lens
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, true));


    // 5. Release the lens temporarily (clear output)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, false));

    // 6. Wait 1.5 seconds
    sequence->AddOperation(std::make_shared<WaitOperation>(1500));

    // 7. Grip the lens again (set output)
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 0, true, 500));

    // 4. Wait for user confirmation that grip is successful
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Grip Confirmation",
      "Confirm left lens is successfully gripped",
      promptUI));

    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4137"));	//see collimate lens

    // 10. Move to placement position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow", "node_5662"));



    return sequence;
  }

  std::unique_ptr<SequenceStep> BuildPickPlaceRightLensSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Pick and Place Right Lens", machineOps);



    // Move hex-right to pick lens position (verify this is correct for RIGHT lens)
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5245"));

    // Move gantry to see the RIGHT lens pick position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4209"));  // Verify this is correct for focus lens

    // Wait for user confirmation
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Lens Position Check",
      "Check the lens position before gripping & click confirm",
      promptUI));

    // Use R-gripper (pin 2) for RIGHT lens
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, true));  // Set RIGHT gripper



    // Release and re-grip cycle for the RIGHT lens
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, false));  // Release RIGHT gripper
    sequence->AddOperation(std::make_shared<WaitOperation>(1500));

    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", 2, true, 500));  // Re-grip RIGHT lens

    // 4. Wait for user confirmation that grip is successful
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Grip Confirmation",
      "Confirm right lens is successfully gripped",
      promptUI));

    // Move gantry to see the RIGHT lens
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow", "node_4156"));  // Verify this is for focus lens

    // Move to RIGHT lens placement position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow", "node_5263"));  // Verify this is correct for RIGHT placement

    return sequence;
  }

}