# How to Create a New Parameterized Process

This guide shows you how to add a new process that works with the recipe system.

## Example: Creating "Core_InspectOnly"

We'll create a parameterized inspection process as an example.

---

## Step 1: Define the Core Operation Class

First, ensure you have the actual operation that does the work.

**CoreSequenceStep.h:**
```cpp
class CoreInspect : public OperationBase {
public:
    CoreInspect(const std::string& deviceName,
                float speed,
                const std::string& inspectNode,
                bool enableCameraView,
                const std::string& cameraGantry,
                const std::string& cameraViewNode)
        : m_deviceName(deviceName),
          m_speed(speed),
          m_inspectNode(inspectNode),
          m_enableCameraView(enableCameraView),
          m_cameraGantry(cameraGantry),
          m_cameraViewNode(cameraViewNode) {
    }

    bool Execute() override {
        // Your inspection logic here
        return true;
    }

private:
    std::string m_deviceName;
    float m_speed;
    std::string m_inspectNode;
    bool m_enableCameraView;
    std::string m_cameraGantry;
    std::string m_cameraViewNode;
};
```

---

## Step 2: Create the Builder Function

Add a parameterized builder in **uaa3_process_builders.h**:

```cpp
namespace UAA3ProcessBuilders {
    
    std::unique_ptr<SequenceStep> createCoreInspectOnly(
        MachineOperations& machineOps,
        UserPromptUI& promptUI,
        const std::string& deviceName,
        const std::string& inspectNode,
        const std::string& cameraGantry,
        const std::string& cameraViewNode,
        float speed,
        bool enableCameraView
    );
    
} // namespace UAA3ProcessBuilders
```

Implement it in **uaa3_dev_sequence.cpp**:

```cpp
namespace UAA3ProcessBuilders {

    std::unique_ptr<SequenceStep> createCoreInspectOnly(
        MachineOperations& machineOps,
        UserPromptUI& promptUI,
        const std::string& deviceName,
        const std::string& inspectNode,
        const std::string& cameraGantry,
        const std::string& cameraViewNode,
        float speed,
        bool enableCameraView)
    {
        auto sequence = std::make_unique<SequenceStep>(
            "Core Inspect Only Sequence", 
            machineOps
        );

        sequence->AddOperation(std::make_shared<::CoreInspect>(
            deviceName,
            speed,
            inspectNode,
            enableCameraView,
            cameraGantry,
            cameraViewNode
        ));

        return sequence;
    }

} // namespace UAA3ProcessBuilders
```

---

## Step 3: Register Parameter Schema (For Recipe Editor)

Add to **CoreProcessParameterRegistration.cpp**:

```cpp
void RegisterCoreProcessParameters() {
    // ... existing registrations ...
    
    // === Core_InspectOnly Parameters ===
    ProcessParameterSchema::RegisterProcessSchema("Core_InspectOnly", {
        ParameterDefinition("deviceName", ParameterType::DEVICE_SELECTION,
                          "Inspection_Robot", "Name of the inspection device"),
        ParameterDefinition("speed", ParameterType::DOUBLE,
                          "30.0", "Movement speed (mm/s)"),
        ParameterDefinition("inspectNode", ParameterType::NODE_SELECTION,
                          "Inspection_Point", "Node for inspection operation"),
        ParameterDefinition("enableCameraView", ParameterType::BOOLEAN,
                          "true", "Enable camera view during operation"),
        ParameterDefinition("cameraGantryDevice", ParameterType::DEVICE_SELECTION,
                          "gantry-main", "Camera gantry device name"),
        ParameterDefinition("cameraViewNode", ParameterType::NODE_SELECTION,
                          "Inspection_Camera_View", "Camera view position node")
    });

    ProcessParameterFactory::RegisterParameterInitializer("Core_InspectOnly",
        static_cast<ParameterInitializer>([](ProcessInstance& instance) {
            auto schema = ProcessParameterSchema::GetParametersForProcess("Core_InspectOnly");
            for (const auto& param : schema) {
                instance.parameters[param.name] = param.defaultValue;
            }
        }));
}
```

---

## Step 4: Register Process with Registry (For Execution)

Add to **ProcessInitializer.cpp** in the `RegisterCoreProcesses()` function:

```cpp
void RegisterCoreProcesses() {
    auto& registry = ProcessRegistry::GetInstance();
    
    // ... existing registrations ...
    
    // ========================================================================
    // Core_InspectOnly - PARAMETERIZED VERSION
    // ========================================================================
    registry.RegisterProcessWithParams(
        "Core_InspectOnly",
        "Core",
        "Quality inspection operation with camera verification",
        true,  // requiresUserPromptUI
        [](MachineOperations& ops, UserPromptUI& ui,
           const std::map<std::string, std::string>& params) -> std::unique_ptr<SequenceStep> {
            
            // Helper functions for parameter extraction
            auto getParam = [&params](const std::string& key, const std::string& defaultValue) {
                auto it = params.find(key);
                return (it != params.end()) ? it->second : defaultValue;
            };
            
            auto getFloatParam = [&params](const std::string& key, float defaultValue) {
                auto it = params.find(key);
                if (it != params.end()) {
                    try { return std::stof(it->second); }
                    catch (...) { return defaultValue; }
                }
                return defaultValue;
            };
            
            auto getBoolParam = [&params](const std::string& key, bool defaultValue) {
                auto it = params.find(key);
                if (it != params.end()) {
                    return (it->second == "true" || it->second == "1");
                }
                return defaultValue;
            };
            
            // Extract parameters
            std::string deviceName = getParam("deviceName", "Inspection_Robot");
            std::string inspectNode = getParam("inspectNode", "Inspection_Point");
            std::string cameraGantry = getParam("cameraGantryDevice", "gantry-main");
            std::string cameraViewNode = getParam("cameraViewNode", "Inspection_Camera_View");
            float speed = getFloatParam("speed", 30.0f);
            bool enableCameraView = getBoolParam("enableCameraView", true);
            
            // Call the builder function
            return UAA3ProcessBuilders::createCoreInspectOnly(
                ops, ui, deviceName, inspectNode,
                cameraGantry, cameraViewNode, speed, enableCameraView
            );
        }
    );
}
```

---

## Complete Flow Diagram

```
1. USER CREATES RECIPE
   RecipePageUI → "Add Process Instance" → "Core_InspectOnly"
   ↓
   ProcessParameterSchema provides default parameters
   ↓
   User edits: inspectNode = "my_custom_node"
   ↓
   Save recipe → JSON file

2. USER RUNS RECIPE
   RunPageUI → Load Recipe → Select "Core_InspectOnly" instance
   ↓
   BuildFromRecipeInstance() extracts parameters map
   ↓
   ProcessRegistry::BuildProcessWithParameters("Core_InspectOnly", params)
   ↓
   Lambda extracts: inspectNode = "my_custom_node"
   ↓
   createCoreInspectOnly(..., "my_custom_node", ...)
   ↓
   Creates CoreInspect operation with custom node
   ↓
   Execute() runs with YOUR values
```

---

## Checklist for New Parameterized Process

- [ ] Core operation class exists (e.g., `CoreInspect`)
- [ ] Builder function declared in `uaa3_process_builders.h`
- [ ] Builder function implemented in `uaa3_dev_sequence.cpp`
- [ ] Parameter schema registered in `CoreProcessParameterRegistration.cpp`
- [ ] Process registered with `RegisterProcessWithParams()` in `ProcessInitializer.cpp`
- [ ] Parameter extraction lambda includes all parameters
- [ ] Default values match between schema and extraction

---

## Testing Your New Process

1. **Build and run** the application
2. **Recipe Page** → Click "New Recipe"
3. **Add Process Instance** → Select "Core_InspectOnly"
4. **Edit parameters** (change nodes, speed, etc.)
5. **Save recipe** as "test_inspect.json"
6. **Run Page** → Click "Load Recipe"
7. **Select your recipe** → Click START
8. **Check logs** - Should show your custom parameter values

Expected log output:
```
[INFO] Recipe parameters (6):
[INFO]   deviceName = Inspection_Robot
[INFO]   inspectNode = my_custom_node  ← YOUR VALUE
[INFO]   speed = 45.0                  ← YOUR VALUE
[INFO] MachineOperations: Inspect Node: my_custom_node
```

---

## Common Mistakes to Avoid

1. **Forgetting parameter helpers** - Always include `getParam()`, `getFloatParam()`, `getBoolParam()`
2. **Mismatched parameter names** - Schema uses "inspectNode", lambda must use "inspectNode" (exact match)
3. **Wrong registration method** - Use `RegisterProcessWithParams()`, not `RegisterProcess()`
4. **Missing default values** - Always provide defaults in case recipe doesn't have the parameter
5. **Type mismatches** - If schema says DOUBLE, use `getFloatParam()` not `getParam()`

This pattern works for any parameterized process you want to add to the system.


# UAA3 Dynamic Process System - Implementation Guide

## 📋 Implementation Steps

### Phase 1: Core System Setup (15 minutes)

1. **Add ProcessRegistry.h**
   - Create `ProcessRegistry.h` in your project
   - Copy the content from the first artifact
   - Add to your includes/headers folder

2. **Add ProcessInitializer.cpp**
   - Create `ProcessInitializer.cpp` in your project
   - Copy the content from the second artifact
   - This migrates your existing 12 UAA3 processes

3. **Update RunPageUI.cpp**
   - Add `#include "ProcessRegistry.h"` at the top
   - Replace the `BuildSelectedProcess()` method with the new version
   - Replace the `GetCurrentProcessList()` method with the new version
   - (Copy from the third artifact)

4. **Update ProcessFilterManager**
   - Remove the hardcoded `m_allProcesses` member variable
   - Update `GetAllAvailableProcesses()` to use the registry
   - Update `RenderFilterWindow()` for category support
   - (Copy from the third artifact)

### Phase 2: Integration & Testing (10 minutes)

5. **Include ProcessInitializer**
   - Add `#include "ProcessInitializer.cpp"` to your main application file
   - OR call `UAA3ProcessRegistration::RegisterExistingUAA3Processes()` during startup

6. **Test Existing Functionality**
   - Build and run your application
   - Verify all 12 existing processes still appear as buttons
   - Test process execution works normally
   - Check filter system still works

### Phase 3: Add New Processes (5 minutes per batch)

7. **Create New Process Files**
   - Copy `NewProcesses_Template.cpp` and rename (e.g., `NewProcesses_Manufacturing.cpp`)
   - Change the namespace name to match your category
   - Implement your specific processes using the template pattern
   - Update the AutoRegister section with your process registrations

8. **Include New Process Files**
   - Add `#include "NewProcesses_Manufacturing.cpp"` to your main file
   - OR add the .cpp file to your CMakeLists.txt/project files
   - The processes will auto-register when the application starts

## 🔧 Code Changes Summary

### Files to Modify:
- ✅ **RunPageUI.cpp** - Replace 2 methods, add 1 include
- ✅ **ProcessFilterManager.h** - Remove hardcoded list, update 1 method
- ✅ **ProcessFilterManager.cpp** - Update RenderFilterWindow method
- ✅ **Main application file** - Add 1 include line

### Files to Create:
- ✅ **ProcessRegistry.h** - Core registry system
- ✅ **ProcessInitializer.cpp** - Existing process registration
- ✅ **NewProcesses_[Category].cpp** - Your new processes (as many as needed)

## 🚀 Benefits Achieved

### Before (Current System):
```cpp
// RunPageUI.cpp - 100+ lines of hardcoded if/else
if (m_selectedProcess == "UAA3_Initialization") {
    return UAA3ProcessBuilders::BuildInitializationSequence_uaa3(m_machineOps, *m_promptUI);
}
else if (m_selectedProcess == "UAA3_Probing") {
    return UAA3ProcessBuilders::BuildProbingSequence_uaa3(m_machineOps, *m_promptUI);
}
// ... 100+ more lines for each new process
```

### After (New System):
```cpp
// RunPageUI.cpp - 5 lines total!
std::unique_ptr<SequenceStep> RunPageUI::BuildSelectedProcess() {
    if (ProcessRegistry::GetInstance().HasProcess(m_selectedProcess)) {
        return ProcessRegistry::GetInstance().BuildProcess(m_selectedProcess, m_machineOps, *m_promptUI);
    }
    return nullptr;
}
```

## 📊 Scalability Comparison

| Task | Before | After |
|------|---------|--------|
| Add 1 new process | Modify 3 files manually | Create 1 new file |
| Add 100 new processes | Modify 3 files with 300+ lines | Create 10 new files (10 processes each) |
| Update UI filters | Manual list updates | Automatic discovery |
| Process categorization | Not supported | Automatic with tooltips |
| Maintenance overhead | High (error-prone) | Zero (auto-registration) |

## 🎯 Example: Adding 50 Manufacturing Processes

### Old Way (Error-Prone):
1. Edit `RunPageUI.cpp` - Add 50 if/else blocks (150+ lines)
2. Edit `ProcessFilterManager.h` - Add 50 process names to hardcoded list
3. Risk of typos, missing entries, broken builds
4. Total time: 2-3 hours

### New Way (Foolproof):
1. Create `NewProcesses_Manufacturing.cpp`
2. Implement 50 process functions using template
3. Add 50 registration lines in AutoRegister block
4. Include the file in your project
5. Total time: 30-45 minutes

## 🔍 Testing Checklist

### Phase 1 Testing:
- [ ] Application builds successfully
- [ ] All 12 existing processes appear as buttons
- [ ] Process selection works (button highlighting)
- [ ] Process execution works normally
- [ ] Filter configuration window opens
- [ ] Existing presets still work

### Phase 2 Testing:
- [ ] Process categories appear in filter window
- [ ] Process tooltips show descriptions
- [ ] "Show Core Only" filter button works
- [ ] New processes appear automatically after adding files
- [ ] No manual UI updates needed

### Phase 3 Testing:
- [ ] Add a test process file with 2-3 processes
- [ ] Verify they appear in UI without any manual changes
- [ ] Test process execution
- [ ] Verify filter categorization works
- [ ] Remove test file and verify processes disappear

## 🚨 Common Issues & Solutions

### Issue 1: Processes not appearing
**Cause:** Include file not added to project
**Solution:** Add `#include "ProcessInitializer.cpp"` to main application file

### Issue 2: Build errors
**Cause:** Missing includes or namespace issues
**Solution:** Verify all includes are correct, check namespace syntax

### Issue 3: Filter not working
**Cause:** ProcessFilterManager not updated
**Solution:** Ensure `GetAllAvailableProcesses()` uses registry

### Issue 4: Auto-registration not working
**Cause:** Static initialization order
**Solution:** Explicitly call registration function in main() if needed

## 📈 Future Enhancements

### Possible Extensions:
1. **Process Dependencies** - Define prerequisite processes
2. **Process Validation** - Runtime parameter checking
3. **Process Templates** - Parameterized process generation
4. **Process Monitoring** - Execution time tracking and optimization
5. **Process Documentation** - Auto-generated help system
6. **Process Versioning** - Track process changes over time

### Migration Path for Legacy Processes:
1. Keep existing UAA3ProcessBuilders namespace
2. Gradually migrate legacy processes to registry
3. Add wrapper functions for compatibility
4. Eventually deprecate old hardcoded system

## 🎉 Success Metrics

After implementation, you should achieve:
- ✅ **Zero manual updates** when adding new processes
- ✅ **5-second process addition** (copy, implement, include)
- ✅ **Automatic UI integration** for all new processes
- ✅ **Category-based organization** with filtering
- ✅ **Tooltip documentation** for all processes
- ✅ **Maintainable codebase** with clear separation of concerns

## 📞 Next Steps

1. **Start with Phase 1** - Implement core system
2. **Test thoroughly** - Verify existing functionality
3. **Add first batch** - Create 5-10 test processes
4. **Scale up** - Add your 100+ production processes
5. **Optimize** - Fine-tune categories and descriptions

**Estimated Total Implementation Time: 1-2 hours**
**Maintenance Time Savings: 10+ hours per major update**