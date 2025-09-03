"""Smart Keithley multimeter interface with optimized auto-ranging for alignment."""

import pyvisa
import time
from typing import Optional, Tuple, Dict, List
from enum import Enum


class CurrentRange(Enum):
    """Keithley 6482 current ranges."""
    RANGE_2nA = 2e-9
    RANGE_20nA = 2e-8
    RANGE_200nA = 2e-7
    RANGE_2uA = 2e-6
    RANGE_20uA = 2e-5
    RANGE_200uA = 2e-4
    RANGE_2mA = 2e-3
    RANGE_20mA = 2e-2


class SmartKeithleyMultimeter:
    """Smart interface for Keithley 6482 with intelligent range management."""
    
    def __init__(self, address: str):
        """
        Initialize the smart multimeter interface.
        
        Args:
            address: VISA resource address of the multimeter
        """
        self.address = address
        self.multimeter = None
        self.resource_manager = None
        self.enabled_channels = [1, 2]
        
        # Range management
        self.current_ranges = {1: CurrentRange.RANGE_20mA.value, 
                               2: CurrentRange.RANGE_20mA.value}
        self.use_auto_range = True
        self.range_check_interval = 100  # Check range every N readings
        self.reading_count = {1: 0, 2: 0}
        
        # Performance tracking
        self.overflow_count = {1: 0, 2: 0}
        self.underflow_count = {1: 0, 2: 0}
        
    def connect(self) -> Tuple[bool, str]:
        """
        Connect to the multimeter with optimized settings.
        
        Returns:
            Tuple of (success, message)
        """
        try:
            self.resource_manager = pyvisa.ResourceManager()
            self.multimeter = self.resource_manager.open_resource(self.address)
            
            # Set timeout
            self.multimeter.timeout = 1000
            
            # Clear errors
            self.multimeter.write("*CLS")
            
            # Get ID
            idn = self.multimeter.query("*IDN?")
            
            # Optimize VISA
            if hasattr(self.multimeter, 'read_termination'):
                self.multimeter.read_termination = '\n'
            if hasattr(self.multimeter, 'write_termination'):
                self.multimeter.write_termination = '\n'
            if hasattr(self.multimeter, 'chunk_size'):
                self.multimeter.chunk_size = 102400
                
            return True, f"Connected: {idn.strip()}"
                
        except Exception as e:
            return False, f"Failed to connect: {e}"
    
    def configure_for_alignment(self, initial_range: str = "AUTO") -> bool:
        """
        Configure for optical/electrical alignment with wide dynamic range.
        
        Args:
            initial_range: "AUTO", "LOW" (for pA-nA), "MID" (for uA), "HIGH" (for mA)
            
        Returns:
            Success status
        """
        if not self.multimeter:
            return False
            
        try:
            # Reset
            self.multimeter.write("*RST")
            time.sleep(0.5)
            
            # Set to current measurement
            self.multimeter.write(":SENS:FUNC 'CURR'")
            
            # Configure channels
            if len(self.enabled_channels) == 2:
                self.multimeter.write(":ROUT:SCAN (@1:2)")
                self.multimeter.write(":ROUT:SCAN:LSEL INT")
            
            # Set initial range based on expected starting point
            if initial_range == "LOW":
                # Start with high sensitivity for finding initial signal
                self._set_range(CurrentRange.RANGE_2uA.value)
                self.multimeter.write(":SENS:CURR:NPLC 1")  # Higher NPLC for low currents
            elif initial_range == "MID":
                self._set_range(CurrentRange.RANGE_200uA.value)
                self.multimeter.write(":SENS:CURR:NPLC 0.1")
            elif initial_range == "HIGH":
                self._set_range(CurrentRange.RANGE_20mA.value)
                self.multimeter.write(":SENS:CURR:NPLC 0.01")  # Fast for high currents
            else:
                # Auto-range mode (slower but handles full range)
                self.multimeter.write(":SENS:CURR:RANG:AUTO ON")
                self.multimeter.write(":SENS:CURR:NPLC 0.1")
                self.use_auto_range = True
                
            # Optimization settings
            self.multimeter.write(":FORM:ELEM READ")  # Only reading, no timestamp
            
            # For alignment, we want some filtering for stability
            self.multimeter.write(":SENS:CURR:AVER:STAT ON")
            self.multimeter.write(":SENS:CURR:AVER:COUN 3")  # Average 3 readings
            self.multimeter.write(":SENS:CURR:AVER:TCON REP")  # Repeating filter
            
            # Keep auto-zero ON for accuracy during alignment
            self.multimeter.write(":SYST:AZER:STAT ON")
            
            # Display can stay on for monitoring
            try:
                self.multimeter.write(":DISP:ENAB ON")
            except:
                pass
                
            return True
            
        except Exception as e:
            print(f"Configuration failed: {e}")
            return False
    
    def configure_fast_mode(self) -> bool:
        """
        Switch to fast mode after initial alignment (when signal is strong).
        """
        if not self.multimeter:
            return False
            
        try:
            # Fast settings for production/monitoring
            self.multimeter.write(":SENS:CURR:NPLC 0.01")  # Fastest
            self.multimeter.write(":SYST:AZER:STAT OFF")   # No auto-zero
            self.multimeter.write(":SENS:CURR:AVER:STAT OFF")  # No averaging
            
            # Fix range at 20mA for your application
            self._set_range(CurrentRange.RANGE_20mA.value)
            
            print("Switched to fast mode (20mA range, NPLC=0.01)")
            return True
            
        except Exception as e:
            print(f"Fast mode configuration failed: {e}")
            return False
    
    def _set_range(self, range_value: float):
        """Set measurement range."""
        self.multimeter.write(":SENS:CURR:RANG:AUTO OFF")
        self.multimeter.write(f":SENS:CURR:RANG {range_value}")
        for ch in self.enabled_channels:
            self.current_ranges[ch] = range_value
    
    def read_with_smart_ranging(self) -> Dict[int, Tuple[Optional[float], str]]:
        """
        Read measurements with intelligent range management.
        
        Returns:
            Dict with channel as key, tuple of (value, range_info) as value
        """
        if not self.multimeter:
            return {ch: (None, "Disconnected") for ch in self.enabled_channels}
            
        try:
            # Read measurement
            response = self.multimeter.query(":READ?")
            
            # Check for overflow/underflow indicators
            if 'OVFL' in response or '+9.9E37' in response:
                # Overflow - need higher range
                self._handle_overflow()
                return self.read_with_smart_ranging()  # Retry with new range
                
            # Parse values
            values = {}
            range_info = {}
            
            if ',' in response:
                parts = response.split(',')
                for i, ch in enumerate(self.enabled_channels):
                    if i < len(parts):
                        try:
                            value = float(parts[i])
                            values[ch] = value
                            
                            # Determine if we're in optimal range
                            range_info[ch] = self._check_optimal_range(ch, value)
                            
                        except ValueError:
                            values[ch] = None
                            range_info[ch] = "Error"
            else:
                try:
                    value = float(response)
                    ch = self.enabled_channels[0]
                    values[ch] = value
                    range_info[ch] = self._check_optimal_range(ch, value)
                except ValueError:
                    values[self.enabled_channels[0]] = None
                    range_info[self.enabled_channels[0]] = "Error"
            
            # Update reading counts
            for ch in self.enabled_channels:
                self.reading_count[ch] += 1
                
                # Periodic range optimization
                if self.reading_count[ch] % self.range_check_interval == 0:
                    if ch in values and values[ch] is not None:
                        self._optimize_range(ch, values[ch])
            
            # Return values with range info
            result = {}
            for ch in self.enabled_channels:
                if ch in values:
                    result[ch] = (values[ch], range_info.get(ch, "Unknown"))
                else:
                    result[ch] = (None, "No data")
                    
            return result
            
        except Exception as e:
            print(f"Read error: {e}")
            return {ch: (None, "Error") for ch in self.enabled_channels}
    
    def _check_optimal_range(self, channel: int, value: float) -> str:
        """
        Check if current value is in optimal range.
        
        Returns range status string.
        """
        if value is None:
            return "Invalid"
            
        current_range = self.current_ranges[channel]
        abs_value = abs(value)
        
        # Check if we're using a good portion of the range (10% to 90%)
        if abs_value < current_range * 0.1:
            if current_range > CurrentRange.RANGE_2nA.value:
                return "Underrange"
        elif abs_value > current_range * 0.9:
            if current_range < CurrentRange.RANGE_20mA.value:
                return "Near overflow"
        
        # Format range for display
        if current_range >= 1e-3:
            return f"{current_range*1000:.0f}mA range"
        elif current_range >= 1e-6:
            return f"{current_range*1e6:.0f}μA range"
        else:
            return f"{current_range*1e9:.0f}nA range"
    
    def _optimize_range(self, channel: int, value: float):
        """
        Optimize range based on measured value.
        """
        if self.use_auto_range:
            return  # Let auto-range handle it
            
        abs_value = abs(value)
        current_range = self.current_ranges[channel]
        
        # Find optimal range (value should be 10-90% of range)
        optimal_range = None
        for range_enum in CurrentRange:
            range_val = range_enum.value
            if abs_value <= range_val * 0.9:
                optimal_range = range_val
                break
        
        if optimal_range and optimal_range != current_range:
            print(f"Channel {channel}: Switching range from {current_range} to {optimal_range}")
            self._set_range(optimal_range)
    
    def _handle_overflow(self):
        """Handle overflow by increasing range."""
        # Switch to highest range
        self._set_range(CurrentRange.RANGE_20mA.value)
        print("Overflow detected - switched to 20mA range")
    
    def get_fast_measurement(self) -> Dict[int, Optional[float]]:
        """
        Fast measurement without range checking (use after alignment).
        """
        if not self.multimeter:
            return {ch: None for ch in self.enabled_channels}
            
        try:
            response = self.multimeter.query(":READ?")
            values = {}
            
            if ',' in response:
                parts = response.split(',')
                for i, ch in enumerate(self.enabled_channels):
                    if i < len(parts):
                        try:
                            values[ch] = float(parts[i])
                        except:
                            values[ch] = None
            else:
                try:
                    values[self.enabled_channels[0]] = float(response)
                except:
                    values[self.enabled_channels[0]] = None
                    
            for ch in self.enabled_channels:
                if ch not in values:
                    values[ch] = None
                    
            return values
            
        except Exception as e:
            return {ch: None for ch in self.enabled_channels}
    
    def close(self):
        """Close connection."""
        if self.multimeter:
            try:
                # Reset to auto-range before closing
                self.multimeter.write(":SENS:CURR:RANG:AUTO ON")
                self.multimeter.write(":SYST:AZER:STAT ON")
                self.multimeter.close()
            except:
                pass
        
        if self.resource_manager:
            try:
                self.resource_manager.close()
            except:
                pass