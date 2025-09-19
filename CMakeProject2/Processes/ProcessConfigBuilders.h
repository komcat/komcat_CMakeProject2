// ProcessConfigBuilders.h
#pragma once
#include "ProcessConfiguration.h"

namespace UAA3ProcessBuilders {

  class ProcessConfigBuilders {
  public:
    // Pick & Place Configuration
    static ProcessConfiguration createPickPlaceConfig() {
      ProcessConfiguration config("PickPlace");

      // Hex-left positions
      config.addNode("hex-left", "pick", "node_5647");
      config.addNode("hex-left", "place", "node_5662");
      config.addNode("hex-left", "home", "node_5480");

      // Hex-right positions
      config.addNode("hex-right", "pick", "node_5245");
      config.addNode("hex-right", "place", "node_5263");
      config.addNode("hex-right", "home", "node_5136");

      // Gantry positions
      config.addNode("gantry", "left-pick-view", "node_4186");
      config.addNode("gantry", "left-place-view", "node_4137");
      config.addNode("gantry", "right-pick-view", "node_4209");
      config.addNode("gantry", "right-place-view", "node_4156");
      config.addNode("gantry", "safe", "node_4027");

      // Gripper parameters
      config.addParameter("gripper", "left-pin", "0", "int");
      config.addParameter("gripper", "right-pin", "2", "int");
      config.addParameter("gripper", "release-wait-ms", "1500", "int");
      config.addParameter("gripper", "regrip-wait-ms", "500", "int");

      return config;
    }

    // Reject Configuration
    static ProcessConfiguration createRejectConfig() {
      ProcessConfiguration config("Reject");

      config.addNode("hex-left", "reject", "node_5531");
      config.addNode("hex-left", "approach", "approachlensplace");
      config.addNode("hex-left", "home", "node_5480");

      config.addNode("hex-right", "reject", "node_5190");
      config.addNode("hex-right", "approach", "approachlensplace");
      config.addNode("hex-right", "home", "node_5136");

      config.addNode("gantry", "safe", "node_4027");

      config.addParameter("timing", "wait-before-release-ms", "1000", "int");
      config.addParameter("timing", "wait-after-release-ms", "1000", "int");

      return config;
    }

    // UV Curing Configuration
    static ProcessConfiguration createUVCuringConfig() {
      ProcessConfiguration config("UVCuring");

      config.addNode("gantry", "uv-position", "node_4426");
      config.addNode("gantry", "safe", "node_4027");
      config.addNode("hex-left", "home", "node_5480");
      config.addNode("hex-right", "home", "node_5136");

      config.addParameter("laser", "high-current-mA", "0.250", "float");
      config.addParameter("laser", "low-current-mA", "0.150", "float");

      config.addParameter("timing", "uv-curing-ms", "210000", "int");
      config.addParameter("timing", "monitor-interval-ms", "5000", "int");
      config.addParameter("timing", "settling-ms", "300", "int");

      config.addParameter("scanning", "step-size-1", "0.0002", "float");
      config.addParameter("scanning", "step-size-2", "0.0001", "float");

      config.addParameter("io", "uv-plc-pin", "14", "int");
      config.addParameter("io", "vacuum-base-pin", "10", "int");

      return config;
    }

    // Probing Configuration
    static ProcessConfiguration createProbingConfig() {
      ProcessConfiguration config("Probing");

      config.addNode("gantry", "view-sled", "node_4083");
      config.addNode("gantry", "view-pic", "node_4107");
      config.addNode("gantry", "safe", "node_4027");
      config.addNode("hex-right", "pic-position", "node_5211");

      config.addParameter("tec", "target-temp-c", "25.0", "float");
      config.addParameter("tec", "tolerance-c", "1.0", "float");
      config.addParameter("tec", "stabilize-ms", "5000", "int");

      config.addParameter("laser", "probing-current-mA", "0.250", "float");

      config.addParameter("io", "vacuum-base-pin", "10", "int");

      return config;
    }
  };

} // namespace