#include "../uaa3_process_builders.h"
#include "SequenceStepMockup.h"
#include "PickAndPlaceParam.h"  // Include your parameter class
#include "CoreSequenceStep.h"
#include <iostream>

namespace UAA3ProcessBuilders {

  // Forward declaration
  void SetupDefaultPickAndPlaceParams();

  std::unique_ptr<SequenceStep> BuildDevSequence_uaa3(
    MachineOperations& machineOps, UserPromptUI& promptUI) {


    // Check if parameters exist before using them
    AppSettings& settings = AppSettings::getInstance();

    if (!settings.exists("PickAndPlace_Robot_Left", "deviceName")) {
      std::cout << "Setting up default parameters for first-time use..." << std::endl;
      SetupDefaultPickAndPlaceParams();
    }


    auto sequence = std::make_unique<SequenceStep>("UAA3 Dev Sequence", machineOps);

    // Task 1
    sequence->AddOperation(std::make_shared<DoSomethingA>("task 1"));

    // Left robot pick and place - loads from AppSettings database
    PickAndPlaceParam param_left("Robot_Left");  // Creates category "PickAndPlace_Robot_Left"
    // Parameters are automatically loaded from database on construction
    // You can optionally modify them here:
    // param_left.setSpeed(120.0);
    // param_left.setNodePick("Left_Station_A");
    // param_left.setNodePlace("Assembly_Point");

    sequence->AddOperation(std::make_shared<PickAndPlaceMockup>(param_left));

    // Task 2
    sequence->AddOperation(std::make_shared<DoSomethingA>("task 2"));

    // Right robot pick and place - loads from AppSettings database  
    PickAndPlaceParam param_right("Robot_Right"); // Creates category "PickAndPlace_Robot_Right"
    // Parameters are automatically loaded from database on construction
    // You can optionally modify them here:
    // param_right.setSpeed(100.0);
    // param_right.setNodePick("Right_Station_B");
    // param_right.setNodePlace("Assembly_Point");

    sequence->AddOperation(std::make_shared<PickAndPlaceMockup>(param_right));

    // Task 3
    sequence->AddOperation(std::make_shared<DoSomethingA>("task 3"));

    return sequence;
  }

  // Alternative: Pre-configured version for specific setups
  std::unique_ptr<SequenceStep> BuildDevSequence_uaa3_Configured(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("UAA3 Dev Sequence (Configured)", machineOps);

    sequence->AddOperation(std::make_shared<DoSomethingA>("Initialize System"));

    // Configure left robot with specific settings
    PickAndPlaceParam param_left("Robot_Left");
    param_left.setDeviceName("Left Arm Assembly Robot");
    param_left.setSpeed(150.0);
    param_left.setNodePick("Input_Conveyor_Left");
    param_left.setNodePlace("Assembly_Station_A");
    param_left.setEnabled(true);

    sequence->AddOperation(std::make_shared<PickAndPlaceMockup>(param_left));

    sequence->AddOperation(std::make_shared<DoSomethingA>("Verify Left Assembly"));

    // Configure right robot with specific settings
    PickAndPlaceParam param_right("Robot_Right");
    param_right.setDeviceName("Right Arm Assembly Robot");
    param_right.setSpeed(130.0);
    param_right.setNodePick("Input_Conveyor_Right");
    param_right.setNodePlace("Assembly_Station_B");
    param_right.setEnabled(true);

    sequence->AddOperation(std::make_shared<PickAndPlaceMockup>(param_right));

    sequence->AddOperation(std::make_shared<DoSomethingA>("Final Quality Check"));

    return sequence;
  }


  // Utility function to setup default parameters for first-time use
  void SetupDefaultPickAndPlaceParams() {
    // This will create default parameters in the database if they don't exist
    PickAndPlaceParam left_defaults("Robot_Left");
    left_defaults.setDeviceName("Left Assembly Robot");
    left_defaults.setSpeed(100.0);
    left_defaults.setNodePick("PickStation_Left");
    left_defaults.setNodePlace("PlaceStation_Left");
    left_defaults.setEnabled(true);

    PickAndPlaceParam right_defaults("Robot_Right");
    right_defaults.setDeviceName("Right Assembly Robot");
    right_defaults.setSpeed(100.0);
    right_defaults.setNodePick("PickStation_Right");
    right_defaults.setNodePlace("PlaceStation_Right");
    right_defaults.setEnabled(true);

    std::cout << "Default pick and place parameters setup complete." << std::endl;
  }





  std::unique_ptr<SequenceStep> CorePickPlace(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Core Pick & Place Sequence", machineOps);




    return sequence;
  }

  std::unique_ptr<SequenceStep> CorePlaceOnly(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Core Place Only Sequence", machineOps);



    return sequence;
  }


  std::unique_ptr<SequenceStep> CorePickOnly(
    MachineOperations& machineOps, UserPromptUI& promptUI) {

    auto sequence = std::make_unique<SequenceStep>("Core Pick Only Sequence", machineOps);






    return sequence;
  }



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


  std::unique_ptr<SequenceStep> createCorePickPlace(
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
    const std::string& deviceName,
    const std::string& graphName,
    const std::string& pickNode,
    const std::string& placeNode,
    const std::string& cameraGantry,
    const std::string& cameraViewPickNode,
    const std::string& cameraViewPlaceNode,
    const std::string& gripperOutputDevice,
    const std::string& gripperPinName,
    float gripperHoldDelay,
    float speed,
    bool enableCameraView)              // ← ADD THIS
  {
    auto sequence = std::make_unique<SequenceStep>(
      "Core Pick & Place Sequence",
      machineOps
    );

    sequence->AddOperation(std::make_shared<::CorePickPlace>(
      deviceName,
      speed,
      pickNode,
      placeNode,
      enableCameraView,
      cameraGantry,
      cameraViewPickNode,
      cameraViewPlaceNode,
      graphName,
      gripperOutputDevice,
      gripperPinName,
      gripperHoldDelay
    ));

    return sequence;
  }

  // ========================================================================
  // Parameterized builder for Core_PickOnly
  // ========================================================================
  std::unique_ptr<SequenceStep> createCorePickOnly(
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
    const std::string& deviceName,
    const std::string& pickNode,
    const std::string& cameraGantry,
    const std::string& cameraViewNode,
    float speed,
    bool enableCameraView)
  {
    auto sequence = std::make_unique<SequenceStep>(
      "Core Pick Only Sequence",
      machineOps
    );

    sequence->AddOperation(std::make_shared<::CorePick>(
      deviceName,
      speed,
      pickNode,
      enableCameraView,
      cameraGantry,
      cameraViewNode
    ));

    return sequence;
  }

  // ========================================================================
  // Parameterized builder for Core_PlaceOnly
  // ========================================================================
  std::unique_ptr<SequenceStep> createCorePlaceOnly(
    MachineOperations& machineOps,
    UserPromptUI& promptUI,
    const std::string& deviceName,
    const std::string& placeNode,
    const std::string& cameraGantry,
    const std::string& cameraViewNode,
    float speed,
    bool enableCameraView)
  {
    auto sequence = std::make_unique<SequenceStep>(
      "Core Place Only Sequence",
      machineOps
    );

    sequence->AddOperation(std::make_shared<::CorePlace>(
      deviceName,
      speed,
      placeNode,
      enableCameraView,
      cameraGantry,
      cameraViewNode
    ));

    return sequence;
  }


}