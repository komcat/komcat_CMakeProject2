#include "ProcessParameterFactory.h"
#include "ProcessParameterSchema.h"
#include "ProcessInstance.h"  // Use the separate header instead of RecipePageUI.h
#include <iostream>

void RegisterCoreProcessParameters() {

  ProcessParameterSchema::RegisterProcessSchema("Core_PickPlace", {
      ParameterDefinition("deviceName", ParameterType::DEVICE_SELECTION,
                        "Precision_Manipulator", "Pick and place device"),
      ParameterDefinition("graphName", ParameterType::STRING,
                        "Process_Flow", "Graph for motion planning"),
      ParameterDefinition("speed", ParameterType::DOUBLE,
                        "10.0", "Movement speed (mm/s)"),
      ParameterDefinition("pickNode", ParameterType::NODE_SELECTION,
                        "node_5245", "Pick position node"),
      ParameterDefinition("placeNode", ParameterType::NODE_SELECTION,
                        "node_5263", "Place position node"),
      ParameterDefinition("enableCameraView", ParameterType::BOOLEAN,
                        "true", "Enable camera view"),
      ParameterDefinition("cameraGantryDevice", ParameterType::DEVICE_SELECTION,
                        "gantry-main", "Camera gantry device"),
      ParameterDefinition("cameraViewPickNode", ParameterType::NODE_SELECTION,
                        "node_4209", "Camera view for pick"),
      ParameterDefinition("cameraViewPlaceNode", ParameterType::NODE_SELECTION,
                        "node_4209", "Camera view for place"),
      ParameterDefinition("gripperOutputDevice", ParameterType::DEVICE_SELECTION,
                        "IOBottom", "Gripper output device"),
      ParameterDefinition("gripperPinName", ParameterType::STRING,       // Changed
                        "gripper-right", "Gripper pin name (e.g., gripper-left, gripper-right)"),
      ParameterDefinition("gripperHoldDelay", ParameterType::DOUBLE,
                        "1.5", "Gripper hold delay in seconds")
    });

  ProcessParameterFactory::RegisterParameterInitializer("Core_PickPlace",
    static_cast<ParameterInitializer>([](ProcessInstance& instance) {
    auto schema = ProcessParameterSchema::GetParametersForProcess("Core_PickPlace");
    for (const auto& param : schema) {
      instance.parameters[param.name] = param.defaultValue;
    }
  }));

  // === Core_PlaceOnly Parameters ===
  ProcessParameterSchema::RegisterProcessSchema("Core_PlaceOnly", {
      ParameterDefinition("deviceName", ParameterType::DEVICE_SELECTION,
                        "Quality_Robot", "Name of the place device"),
      ParameterDefinition("speed", ParameterType::DOUBLE,
                        "60.0", "Movement speed (mm/s)"),
      ParameterDefinition("placeNode", ParameterType::NODE_SELECTION,
                        "QC_Station", "Node for placing operation"),
      ParameterDefinition("enableCameraView", ParameterType::BOOLEAN,
                        "true", "Enable camera view during operation"),
      ParameterDefinition("cameraGantryDevice", ParameterType::DEVICE_SELECTION,
                        "gantry-main", "Camera gantry device name"),
      ParameterDefinition("cameraViewNode", ParameterType::NODE_SELECTION,
                        "QC_Camera_Position", "Camera view position node")
    });

  // Fix: Explicitly cast lambda to ParameterInitializer
  ProcessParameterFactory::RegisterParameterInitializer("Core_PlaceOnly",
    static_cast<ParameterInitializer>([](ProcessInstance& instance) {
    auto schema = ProcessParameterSchema::GetParametersForProcess("Core_PlaceOnly");
    for (const auto& param : schema) {
      instance.parameters[param.name] = param.defaultValue;
    }
  }));

  // === Core_PickOnly Parameters ===
  ProcessParameterSchema::RegisterProcessSchema("Core_PickOnly", {
      ParameterDefinition("deviceName", ParameterType::DEVICE_SELECTION,
                        "Precision_Manipulator", "Name of the pick device"),
      ParameterDefinition("speed", ParameterType::DOUBLE,
                        "10.0", "Movement speed (mm/s)"),
      ParameterDefinition("pickNode", ParameterType::NODE_SELECTION,
                        "Precision_Pick_Point", "Node for picking operation"),
      ParameterDefinition("enableCameraView", ParameterType::BOOLEAN,
                        "true", "Enable camera view during operation"),
      ParameterDefinition("cameraGantryDevice", ParameterType::DEVICE_SELECTION,
                        "gantry-main", "Camera gantry device name"),
      ParameterDefinition("cameraViewNode", ParameterType::NODE_SELECTION,
                        "Precision_View_Point", "Camera view position node")
    });

  ProcessParameterFactory::RegisterParameterInitializer("Core_PickOnly",
    static_cast<ParameterInitializer>([](ProcessInstance& instance) {
    auto schema = ProcessParameterSchema::GetParametersForProcess("Core_PickOnly");
    for (const auto& param : schema) {
      instance.parameters[param.name] = param.defaultValue;
    }
  }));

  // === Core_PlaceOnly Parameters ===
  ProcessParameterSchema::RegisterProcessSchema("Core_PlaceOnly", {
      ParameterDefinition("deviceName", ParameterType::DEVICE_SELECTION,
                        "hex-right", "Name of the place device"),
      ParameterDefinition("speed", ParameterType::DOUBLE,
                        "10.0", "Movement speed (mm/s)"),
      ParameterDefinition("placeNode", ParameterType::NODE_SELECTION,
                        "node_5263", "Node for placing operation"),
      ParameterDefinition("enableCameraView", ParameterType::BOOLEAN,
                        "true", "Enable camera view during operation"),
      ParameterDefinition("cameraGantryDevice", ParameterType::DEVICE_SELECTION,
                        "gantry-main", "Camera gantry device name"),
      ParameterDefinition("cameraViewNode", ParameterType::NODE_SELECTION,
                        "QC_Camera_Position", "Camera view position node")
    });

  ProcessParameterFactory::RegisterParameterInitializer("Core_PlaceOnly",
    static_cast<ParameterInitializer>([](ProcessInstance& instance) {
    auto schema = ProcessParameterSchema::GetParametersForProcess("Core_PlaceOnly");
    for (const auto& param : schema) {
      instance.parameters[param.name] = param.defaultValue;
    }
  }));




  // === Core_UVOnly Parameters ===
  ProcessParameterSchema::RegisterProcessSchema("Core_UVOnly", {
      ParameterDefinition("deviceName", ParameterType::DEVICE_SELECTION,
                        "gantry-main", "Device to move to UV position"),
      ParameterDefinition("graphName", ParameterType::STRING,
                        "Process_Flow", "Graph for motion planning"),
      ParameterDefinition("uvNode", ParameterType::NODE_SELECTION,
                        "node_4426", "UV curing position node"),
      ParameterDefinition("pneumaticUVDevice", ParameterType::DEVICE_SELECTION,
                        "UV_Head", "Pneumatic UV head device"),
      ParameterDefinition("ioDevice", ParameterType::DEVICE_SELECTION,
                        "IOBottom", "IO device for UV trigger"),
      ParameterDefinition("uvTriggerPinName", ParameterType::STRING,
                        "uv-plc-trigger", "UV trigger pin name"),
      ParameterDefinition("uvDurationSeconds", ParameterType::DOUBLE,
                        "210.0", "UV curing duration in seconds"),
      ParameterDefinition("speed", ParameterType::DOUBLE,
                        "5.0", "Movement speed (mm/s)"),
      // Fine alignment 1
      ParameterDefinition("fineAlignmentEnable1", ParameterType::BOOLEAN,
                        "true", "Enable fine alignment 1"),
      ParameterDefinition("fineAlignmentDevice1", ParameterType::DEVICE_SELECTION,
                        "hex-left", "Fine alignment device 1"),
      ParameterDefinition("feedBackChannelName1", ParameterType::STRING,
                        "GPIB-Current", "Feedback channel for alignment 1"),
      // Fine alignment 2
      ParameterDefinition("fineAlignmentEnable2", ParameterType::BOOLEAN,
                        "true", "Enable fine alignment 2"),
      ParameterDefinition("fineAlignmentDevice2", ParameterType::DEVICE_SELECTION,
                        "hex-right", "Fine alignment device 2"),
      ParameterDefinition("feedBackChannelName2", ParameterType::STRING,
                        "GPIB-Current", "Feedback channel for alignment 2")
  });

  ProcessParameterFactory::RegisterParameterInitializer("Core_UVOnly",
    static_cast<ParameterInitializer>([](ProcessInstance& instance) {
    auto schema = ProcessParameterSchema::GetParametersForProcess("Core_UVOnly");
    for (const auto& param : schema) {
      instance.parameters[param.name] = param.defaultValue;
    }
  }));


  std::cout << "Core process parameters registered successfully" << std::endl;
}