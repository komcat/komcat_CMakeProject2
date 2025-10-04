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


  std::cout << "Core process parameters registered successfully" << std::endl;
}