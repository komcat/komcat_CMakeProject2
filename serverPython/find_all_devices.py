import pyvisa
import sys

def find_gpib_devices():
    """
    Scan for and print all available GPIB devices
    """
    try:
        # Create a resource manager
        rm = pyvisa.ResourceManager()
        
        # Get list of all available resources
        resources = rm.list_resources()
        
        if not resources:
            print("No VISA resources found.")
            return
        
        print("All VISA Resources:")
        print("-" * 40)
        for resource in resources:
            print(f"  {resource}")
        
        # Filter for GPIB devices specifically
        gpib_devices = [res for res in resources if 'GPIB' in res.upper()]
        
        if not gpib_devices:
            print("\nNo GPIB devices found.")
            return
        
        print(f"\nFound {len(gpib_devices)} GPIB device(s):")
        print("-" * 40)
        
        for device in gpib_devices:
            try:
                # Try to connect and get device info
                instrument = rm.open_resource(device)
                instrument.timeout = 2000  # 2 second timeout
                
                try:
                    # Try to get device identification
                    idn = instrument.query('*IDN?').strip()
                    print(f"  {device}: {idn}")
                except:
                    print(f"  {device}: (Device found but no response to *IDN?)")
                
                instrument.close()
                
            except Exception as e:
                print(f"  {device}: (Error connecting - {str(e)})")
    
    except ImportError:
        print("Error: pyvisa library not found.")
        print("Install it with: pip install pyvisa")
        sys.exit(1)
    
    except Exception as e:
        print(f"Error initializing VISA: {str(e)}")
        print("Make sure you have VISA runtime installed (NI-VISA, Keysight IO Libraries, etc.)")
        sys.exit(1)

def scan_gpib_addresses():
    """
    Alternative method: Scan specific GPIB addresses (0-30) on primary GPIB interface
    """
    try:
        rm = pyvisa.ResourceManager()
        print("\nScanning GPIB addresses 0-30 on primary interface:")
        print("-" * 50)
        
        found_devices = []
        
        for addr in range(31):  # GPIB addresses 0-30
            resource_name = f"GPIB0::{addr}::INSTR"
            try:
                instrument = rm.open_resource(resource_name)
                instrument.timeout = 1000  # 1 second timeout for scanning
                
                try:
                    idn = instrument.query('*IDN?').strip()
                    print(f"  Address {addr:2d}: {idn}")
                    found_devices.append((addr, idn))
                except:
                    # Device responds but not to *IDN?
                    print(f"  Address {addr:2d}: Device present (no *IDN? response)")
                    found_devices.append((addr, "Unknown device"))
                
                instrument.close()
                
            except:
                # No device at this address (expected for most addresses)
                pass
        
        if not found_devices:
            print("  No devices found on GPIB addresses 0-30")
        else:
            print(f"\nSummary: Found {len(found_devices)} device(s)")
            
    except Exception as e:
        print(f"Error during address scan: {str(e)}")

if __name__ == "__main__":
    print("GPIB Device Scanner")
    print("=" * 40)
    
    # Method 1: Use VISA resource discovery
    find_gpib_devices()
    
    # Method 2: Scan GPIB addresses directly
    scan_gpib_addresses()
    
    print("\nScan complete.")