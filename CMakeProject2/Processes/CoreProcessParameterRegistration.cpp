#include "ProcessParameterFactory.h"
#include "ProcessParameterSchema.h"
#include "ProcessInstance.h"  // Use the separate header instead of RecipePageUI.h
#include <iostream>

void RegisterCoreProcessParameters() {

  // === Core_PickPlace Parameters ===
  ProcessParameterSchema::RegisterProcessSchema("Core_PickPlace", {
      ParameterDefinition("deviceName", ParameterType::DEVICE_SELECTION,
                        "Precision_Manipulator", "Name of the pick and place device"),
      ParameterDefinition("speed", ParameterType::DOUBLE,
                        "50.0", "Movement speed (mm/s)"),
      ParameterDefinition("pickNode", ParameterType::NODE_SELECTION,
                        "Precision_Pick_Point", "Node for picking operation"),
      ParameterDefinition("placeNode", ParameterType::NODE_SELECTION,
                        "Final_Assembly", "Node for placing operation"),
      ParameterDefinition("enableCameraView", ParameterType::BOOLEAN,
                        "true", "Enable camera view during operation"),
      ParameterDefinition("cameraGantryDevice", ParameterType::DEVICE_SELECTION,
                        "gantry-main", "Camera gantry device name"),
      ParameterDefinition("cameraViewNode", ParameterType::NODE_SELECTION,
                        "Precision_View_Point", "Camera view position node")
    });

  // Fix: Explicitly cast lambda to ParameterInitializer
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

  std::cout << "Core process parameters registered successfully" << std::endl;
}