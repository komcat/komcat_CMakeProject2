"""Keithley multimeter interface module."""

import pyvisa
from typing import Optional, Tuple


class KeithleyMultimeter:
    """Interface for Keithley multimeter operations."""
    
    def __init__(self, address: str):
        """
        Initialize the multimeter interface.
        
        Args:
            address: VISA resource address of the multimeter
        """
        self.address = address
        self.multimeter = None
        self.resource_manager = None
        
    def connect(self) -> Tuple[bool, str]:
        """
        Connect to the multimeter.
        
        Returns:
            Tuple of (success, message)
        """
        try:
            self.resource_manager = pyvisa.ResourceManager()
            self.multimeter = self.resource_manager.open_resource(self.address)
            return True, "Multimeter connected successfully"
        except Exception as e:
            return False, f"Failed to connect to multimeter: {e}"
    
    def configure(self, function: str = 'CURR') -> bool:
        """
        Configure the multimeter measurement function.
        
        Args:
            function: Measurement function (e.g., 'CURR' for current)
            
        Returns:
            Success status
        """
        if not self.multimeter:
            return False
            
        try:
            self.multimeter.write(f":SENS:FUNC '{function}'")
            return True
        except Exception as e:
            print(f"Failed to configure multimeter: {e}")
            return False
    
    def read_measurement(self) -> Optional[float]:
        """
        Read a single measurement from channel 1.
        
        Returns:
            Measurement value or None if error
        """
        if not self.multimeter:
            return None
            
        try:
            response = self.multimeter.query(":READ?")
            data_str = response.strip()
            
            # Extract channel 1 value
            if ',' in data_str:
                return float(data_str.split(',')[0])
            else:
                return float(data_str)
                
        except (pyvisa.errors.VisaIOError, ValueError) as e:
            print(f"Error reading measurement: {e}")
            return None
        except Exception as e:
            print(f"Unexpected error: {e}")
            return None
    
    def close(self):
        """Close the multimeter connection."""
        if self.multimeter:
            try:
                self.multimeter.close()
            except:
                pass
        
        if self.resource_manager:
            try:
                self.resource_manager.close()
            except:
                pass