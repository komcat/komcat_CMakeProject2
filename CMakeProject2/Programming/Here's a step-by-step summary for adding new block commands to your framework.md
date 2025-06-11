# Framework Guide: Adding New Block Commands

## Overview
This guide shows how to add new block commands to the visual block programming system. We successfully added `EXTEND_SLIDE` and `RETRACT_SLIDE` commands as examples.

## Step-by-Step Process

### Step 1: Add Block Type to Enum
**File: `MachineBlockUI.h`**
```cpp
enum class BlockType {
  START,
  END,
  MOVE_NODE,
  WAIT,
  SET_OUTPUT,
  CLEAR_OUTPUT,
  EXTEND_SLIDE,    // NEW: Your new block type
  RETRACT_SLIDE    // NEW: Another new block type
};
```

### Step 2: Add Block Colors
**File: `MachineBlockUI.h`**
```cpp
const ImU32 EXTEND_SLIDE_COLOR = IM_COL32(100, 200, 100, 255);   // Light green
const ImU32 RETRACT_SLIDE_COLOR = IM_COL32(200, 100, 100, 255);  // Light red
```

### Step 3: Update Color Mapping
**File: `MachineBlockUI.cpp` - `GetBlockColor()` method**
```cpp
case BlockType::EXTEND_SLIDE: return EXTEND_SLIDE_COLOR;
case BlockType::RETRACT_SLIDE: return RETRACT_SLIDE_COLOR;
```

### Step 4: Update String Conversions
**File: `MachineBlockUI.cpp`**

#### `BlockTypeToString()` method:
```cpp
case BlockType::EXTEND_SLIDE: return "Extend Slide";
case BlockType::RETRACT_SLIDE: return "Retract Slide";
```

#### `BlockTypeToJsonString()` method:
```cpp
case BlockType::EXTEND_SLIDE: return "EXTEND_SLIDE";
case BlockType::RETRACT_SLIDE: return "RETRACT_SLIDE";
```

#### `JsonStringToBlockType()` method:
```cpp
if (typeStr == "EXTEND_SLIDE") return BlockType::EXTEND_SLIDE;
if (typeStr == "RETRACT_SLIDE") return BlockType::RETRACT_SLIDE;
```

### Step 5: Add to Palette
**File: `MachineBlockUI.cpp` - `InitializePaletteBlocks()` method**
```cpp
m_paletteBlocks.emplace_back(7, BlockType::EXTEND_SLIDE, "Extend Slide", GetBlockColor(BlockType::EXTEND_SLIDE));
m_paletteBlocks.emplace_back(8, BlockType::RETRACT_SLIDE, "Retract Slide", GetBlockColor(BlockType::RETRACT_SLIDE));
```

### Step 6: Define Block Parameters
**File: `MachineBlockUI.cpp` - `InitializeBlockParameters()` method**
```cpp
case BlockType::EXTEND_SLIDE:
  block.parameters.push_back({"slide_name", "", "string", "Name of the pneumatic slide to extend"});
  break;
  
case BlockType::RETRACT_SLIDE:
  block.parameters.push_back({"slide_name", "", "string", "Name of the pneumatic slide to retract"});
  break;
```

### Step 7: Update Block Labels (Optional)
**File: `MachineBlockUI.cpp` - Add helper method**
```cpp
void MachineBlockUI::UpdateBlockLabel(MachineBlock& block) {
  switch (block.type) {
  case BlockType::EXTEND_SLIDE: {
    std::string slideName = GetParameterValue(block, "slide_name");
    block.label = slideName.empty() ? "Extend Slide" : "Extend\n" + slideName;
    break;
  }
  // ... similar for other block types
  }
}
```

### Step 8: Add Sequence Converter Methods
**File: `BlockSequenceConverter.h`**
```cpp
class BlockSequenceConverter {
private:
  std::shared_ptr<SequenceOperation> ConvertExtendSlideBlock(const MachineBlock& block);
  std::shared_ptr<SequenceOperation> ConvertRetractSlideBlock(const MachineBlock& block);
};
```

**File: `BlockSequenceConverter.cpp`**
```cpp
std::shared_ptr<SequenceOperation> BlockSequenceConverter::ConvertExtendSlideBlock(const MachineBlock& block) {
  std::string slideName = GetParameterValue(block, "slide_name");
  
  if (slideName.empty()) {
    m_machineOps.LogWarning("EXTEND_SLIDE block missing slide_name parameter");
    return nullptr;
  }
  
  return std::make_shared<ExtendSlideOperation>(slideName);
}
```

### Step 9: Update Main Conversion Switch
**File: `BlockSequenceConverter.cpp` - `ConvertBlocksToSequence()` method**
```cpp
switch (block->type) {
  // ... existing cases ...
  
  case BlockType::EXTEND_SLIDE:
    operation = ConvertExtendSlideBlock(*block);
    break;

  case BlockType::RETRACT_SLIDE:
    operation = ConvertRetractSlideBlock(*block);
    break;
}
```

### Step 10: Add Property Renderers (Optional but Recommended)
**File: `BlockPropertyRenderers.h`**
```cpp
class ExtendSlideRenderer : public BlockPropertyRenderer {
public:
  void RenderProperties(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderActions(MachineBlock* block, MachineOperations* machineOps) override;
  void RenderValidation(MachineBlock* block) override;
};
```

**File: `BlockPropertyRenderers.cpp`**
```cpp
// Implement the renderer methods with UI, test buttons, and validation
```

### Step 11: Update Renderer Factory
**File: `BlockPropertyRenderers.cpp` - `BlockRendererFactory::CreateRenderer()` method**
```cpp
case BlockType::EXTEND_SLIDE:
  return std::make_unique<ExtendSlideRenderer>();
case BlockType::RETRACT_SLIDE:
  return std::make_unique<RetractSlideRenderer>();
```

## Prerequisites for New Block Commands

### 1. Sequence Operations Must Exist
Your new block type needs corresponding `SequenceOperation` classes:
```cpp
class YourNewOperation : public SequenceOperation {
public:
  bool Execute(MachineOperations& ops) override;
  std::string GetDescription() const override;
};
```

### 2. Machine Operations Support
The `MachineOperations` interface must have the required methods:
```cpp
class MachineOperations {
public:
  bool YourNewMethod(/* parameters */);
};
```

## Quick Checklist for New Block Types

- [ ] Add to `BlockType` enum
- [ ] Add color constant
- [ ] Update `GetBlockColor()`
- [ ] Update `BlockTypeToString()`
- [ ] Update `BlockTypeToJsonString()`
- [ ] Update `JsonStringToBlockType()`
- [ ] Add to palette in `InitializePaletteBlocks()`
- [ ] Define parameters in `InitializeBlockParameters()`
- [ ] Add converter method declaration in `.h`
- [ ] Implement converter method in `.cpp`
- [ ] Update main conversion switch
- [ ] Create property renderer (optional)
- [ ] Update renderer factory (if using custom renderer)

## Example Block Types You Could Add Next

1. **CAMERA_CAPTURE** - Take photos
2. **MOVE_RELATIVE** - Relative motion commands  
3. **SCAN_OPERATION** - Run scanning sequences
4. **LASER_ON/LASER_OFF** - Laser control
5. **TEC_CONTROL** - Temperature control
6. **DATA_LOG** - Log sensor readings
7. **USER_PROMPT** - Wait for user input
8. **PARALLEL_MOVE** - Move multiple devices simultaneously

## Tips for Success

1. **Keep it simple**: Start with basic parameters
2. **Follow patterns**: Copy existing block implementations
3. **Test incrementally**: Add one block type at a time
4. **Use meaningful colors**: Visual differentiation helps users
5. **Add validation**: Help users catch parameter errors
6. **Provide test buttons**: Let users test individual blocks
7. **Document parameters**: Clear descriptions help users

The framework is now proven and ready for rapid expansion of new block commands!