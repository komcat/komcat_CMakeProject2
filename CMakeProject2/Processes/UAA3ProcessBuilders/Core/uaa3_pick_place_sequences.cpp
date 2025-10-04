// Core/uaa3_pick_place_sequences.cpp
#include "../uaa3_process_builders.h"
#include "UserInputOperations.h"
#include "ManualAdjustmentOperation.h"


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


		//// 11. Manual gripper adjustment
  //  auto gripperAdjustment = std::make_shared<ManualAdjustmentOperation>(
  //    "hex-left",
  //    "Gripper Position Adjustment",
  //    "Use the jog controls to adjust the lens to have first light value >1uA.\n"
  //    "Be careful not to hit something.",
  //    promptUI,
  //    true, true, true  // Only Z axis enabled
  //  );
  //  gripperAdjustment->WithStepSize(0.01)
  //    .WithShowPosition(true);
  //  sequence->AddOperation(gripperAdjustment);


    return sequence;
  }



  std::unique_ptr<SequenceStep> BuildPickPlaceLeftLensSequence_uaa3_Configurable(
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
    const ProcessConfiguration& config) {

    auto sequence = std::make_unique<SequenceStep>("Pick and Place Left Lens", machineOps);

    // Move hex-left to pick lens position - using config
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow",
      config.getNode("hex-left", "pick")));

    // Move gantry to view position - using config
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow",
      config.getNode("gantry", "left-pick-view")));

    // Wait for user confirmation
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Lens Position Check",
      "Check the lens position before gripping & click confirm",
      promptUI));

    // Set output L-gripper to grab the lens - using config
    int leftPin = config.getInt("gripper", "left-pin");
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", leftPin, true));

    // Release the lens temporarily
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", leftPin, false));

    // Wait - using config timing
    sequence->AddOperation(std::make_shared<WaitOperation>(
      config.getInt("gripper", "release-wait-ms")));

    // Grip the lens again
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", leftPin, true,
      config.getInt("gripper", "regrip-wait-ms")));

    // Confirmation
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Grip Confirmation",
      "Confirm left lens is successfully gripped",
      promptUI));

    // Move gantry to place view position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow",
      config.getNode("gantry", "left-place-view")));

    // Move to placement position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-left", "Process_Flow",
      config.getNode("hex-left", "place")));

    return sequence;
  }



  std::unique_ptr<SequenceStep> BuildPickPlaceRightLensSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Pick and Place Right Lens", machineOps);




    /*
    
    hex-right
    pick
      node_5245
    see pick
      node_4209

      gripper right  "IOBottom", 2

    place
			node_5263
    see place
     node_4209

    
    
    */


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

    //// 11. Manual gripper adjustment
    //auto gripperAdjustment = std::make_shared<ManualAdjustmentOperation>(
    //  "hex-right",
    //  "Gripper Position Adjustment",
    //  "Use the jog controls to adjust the lens to have norminal position.\n Value should be >80uA"
    //  "Be careful not to hit something.",
    //  promptUI,
    //  true, true, true  // X Y Z axis enabled
    //);
    //gripperAdjustment->WithStepSize(0.01)
    //  .WithShowPosition(true);
    //sequence->AddOperation(gripperAdjustment);



    return sequence;
  }



  std::unique_ptr<SequenceStep> BuildPickPlaceRightLensSequence_uaa3_Configurable(
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
    const ProcessConfiguration& config) {

    auto sequence = std::make_unique<SequenceStep>("Pick and Place Right Lens", machineOps);

    // Move hex-right to pick lens position - using config
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow",
      config.getNode("hex-right", "pick")));

    // Move gantry to view position - using config
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow",
      config.getNode("gantry", "right-pick-view")));

    // Wait for user confirmation
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Lens Position Check",
      "Check the lens position before gripping & click confirm",
      promptUI));

    // Set output R-gripper to grab the lens - using config
    int rightPin = config.getInt("gripper", "right-pin");
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", rightPin, true));

    // Release the lens temporarily
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", rightPin, false));

    // Wait - using config timing
    sequence->AddOperation(std::make_shared<WaitOperation>(
      config.getInt("gripper", "release-wait-ms")));

    // Grip the lens again
    sequence->AddOperation(std::make_shared<SetOutputOperation>(
      "IOBottom", rightPin, true,
      config.getInt("gripper", "regrip-wait-ms")));

    // Confirmation
    sequence->AddOperation(UserPromptOperation::CreateBasic(
      "Grip Confirmation",
      "Confirm right lens is successfully gripped",
      promptUI));

    // Move gantry to place view position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "gantry-main", "Process_Flow",
      config.getNode("gantry", "right-place-view")));

    // Move to placement position
    sequence->AddOperation(std::make_shared<MoveToNodeOperation>(
      "hex-right", "Process_Flow",
      config.getNode("hex-right", "place")));

    return sequence;
  }


}