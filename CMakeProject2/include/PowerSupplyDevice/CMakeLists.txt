# Add the test executable for Power Supply System
add_executable(test_power_supply 
    test_PowerSupplySystem.cpp
    include/PowerSupplyDevice/PowerSupplyManager.cpp
    include/PowerSupplyDevice/MockPowerSupplyDevice.cpp
    include/FileResultStorage.cpp
)

# Include directories for the test
target_include_directories(test_power_supply PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/PowerSupplyDevice
)

# Link required libraries
target_link_libraries(test_power_supply PRIVATE
    nlohmann_json::nlohmann_json  # For JSON serialization in FileResultStorage
)

# Set C++ standard
set_target_properties(test_power_supply PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)