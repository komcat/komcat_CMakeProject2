#!/usr/bin/env python3
"""
Keithley 2400 SourceMeter Server - Optimized for High-Rate Polling
Provides TCP server interface for remote control of Keithley 2400
"""

import socket
import threading
import json
import time
import pyvisa
import logging
from datetime import datetime

# Setup logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class Keithley2400Server:
    def __init__(self, host='localhost', port=8888, gpib_address='GPIB1::24::INSTR', fast_mode=True):
        self.host = host
        self.port = port
        self.gpib_address = gpib_address
        self.fast_mode = fast_mode  # Enable optimizations for high-rate polling
        self.instrument = None
        self.server_socket = None
        self.running = False
        self.clients = []
        
        # Performance tracking
        self.read_count = 0
        self.error_count = 0
        self.last_stats_time = time.time()
        
        # Connect to instrument
        self.connect_instrument()
        
    def connect_instrument(self):
        """Connect to Keithley 2400"""
        try:
            rm = pyvisa.ResourceManager()
            self.instrument = rm.open_resource(self.gpib_address)
            
            # Optimize timeouts for fast polling
            if self.fast_mode:
                self.instrument.timeout = 1000  # 1 second - faster timeout
                # Configure instrument for faster operation
                self.instrument.write(':SYST:AZER OFF')  # Disable auto-zero for speed
                self.instrument.write(':DISP:ENAB OFF')  # Disable display updates for speed
            else:
                self.instrument.timeout = 5000  # 5 seconds - standard timeout
            
            # Test connection
            idn = self.instrument.query('*IDN?').strip()
            logger.info(f"Connected to: {idn}")
            
            # Initialize instrument
            self.instrument.write('*RST')
            self.instrument.write('*CLS')
            
            if self.fast_mode:
                logger.info("Fast mode enabled - optimized for high-rate polling")
                # Set up for continuous measurements
                self.instrument.write(':SENS:FUNC:CONC ON')  # Concurrent functions
                self.instrument.write(':FORM:ELEM VOLT,CURR')  # Only voltage and current
            
        except Exception as e:
            logger.error(f"Failed to connect to instrument: {e}")
            raise
            
    def start_server(self):
        """Start the TCP server"""
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(5)
            
            self.running = True
            logger.info(f"Keithley 2400 Server started on {self.host}:{self.port}")
            if self.fast_mode:
                logger.info("Server optimized for 250ms polling rate")
            
            # Start statistics reporting thread
            stats_thread = threading.Thread(target=self.stats_reporter)
            stats_thread.daemon = True
            stats_thread.start()
            
            while self.running:
                try:
                    client_socket, address = self.server_socket.accept()
                    logger.info(f"Client connected from {address}")
                    
                    client_thread = threading.Thread(
                        target=self.handle_client,
                        args=(client_socket, address)
                    )
                    client_thread.daemon = True
                    client_thread.start()
                    
                except Exception as e:
                    if self.running:
                        logger.error(f"Error accepting connection: {e}")
                        
        except Exception as e:
            logger.error(f"Server error: {e}")
        finally:
            self.cleanup()
    
    def stats_reporter(self):
        """Report performance statistics every 30 seconds"""
        while self.running:
            time.sleep(30)
            if self.read_count > 0:
                elapsed = time.time() - self.last_stats_time
                rate = self.read_count / elapsed
                error_rate = (self.error_count / self.read_count) * 100 if self.read_count > 0 else 0
                logger.info(f"Performance: {rate:.1f} reads/sec, {error_rate:.1f}% errors")
                
                # Reset counters
                self.read_count = 0
                self.error_count = 0
                self.last_stats_time = time.time()
            
    def handle_client(self, client_socket, address):
        """Handle individual client connection"""
        self.clients.append(client_socket)
        
        try:
            while self.running:
                # Receive command from client
                data = client_socket.recv(1024).decode('utf-8')
                if not data:
                    break
                    
                try:
                    # Parse JSON command
                    command = json.loads(data)
                    # Reduce logging for read commands to avoid spam
                    if command.get('type') != 'read':
                        logger.info(f"Command from {address}: {command}")
                    
                    # Process command
                    response = self.process_command(command)
                    
                    # Send response back to client
                    response_json = json.dumps(response)
                    client_socket.send(response_json.encode('utf-8'))
                    
                except json.JSONDecodeError:
                    error_response = {"status": "error", "message": "Invalid JSON format"}
                    client_socket.send(json.dumps(error_response).encode('utf-8'))
                    
                except Exception as e:
                    error_response = {"status": "error", "message": str(e)}
                    client_socket.send(json.dumps(error_response).encode('utf-8'))
                    logger.error(f"Error processing command: {e}")
                    
        except Exception as e:
            logger.error(f"Client {address} error: {e}")
        finally:
            client_socket.close()
            if client_socket in self.clients:
                self.clients.remove(client_socket)
            logger.info(f"Client {address} disconnected")
            
    def process_command(self, command):
        """Process instrument command"""
        cmd_type = command.get('type', '')
        cmd_data = command.get('data', {})
        
        try:
            if cmd_type == 'write':
                # Write command to instrument
                scpi_cmd = cmd_data.get('command', '')
                self.instrument.write(scpi_cmd)
                return {"status": "success", "message": f"Command '{scpi_cmd}' executed"}
                
            elif cmd_type == 'query':
                # Query instrument
                scpi_cmd = cmd_data.get('command', '')
                result = self.instrument.query(scpi_cmd).strip()
                return {"status": "success", "data": result}
                
            elif cmd_type == 'read':
                # Take measurement - optimized for fast polling
                self.read_count += 1
                start_time = time.time()
                
                try:
                    if self.fast_mode:
                        # Fast method: Read only voltage and current
                        # Use a single query that returns both values
                        result = self.instrument.query(':READ?').strip()
                        values = result.split(',')
                        
                        voltage = float(values[0])
                        current = float(values[1])
                        
                        # Calculate derived values
                        if abs(current) > 1e-12:  # Avoid division by zero
                            resistance = voltage / current
                        else:
                            resistance = 1e9  # Very high resistance for open circuit
                            
                        power = voltage * current
                        
                        measurement = {
                            "voltage": voltage,
                            "current": current,
                            "resistance": resistance,
                            "power": power,
                            "timestamp": datetime.now().isoformat(),
                            "read_time_ms": (time.time() - start_time) * 1000
                        }
                    else:
                        # Standard method: Full read with all values
                        result = self.instrument.query(':READ?').strip()
                        values = result.split(',')
                        measurement = {
                            "voltage": float(values[0]),
                            "current": float(values[1]),
                            "resistance": float(values[2]) if len(values) > 2 else None,
                            "power": float(values[3]) if len(values) > 3 else None,
                            "timestamp": datetime.now().isoformat(),
                            "read_time_ms": (time.time() - start_time) * 1000
                        }
                    
                    return {"status": "success", "data": measurement}
                    
                except Exception as e:
                    self.error_count += 1
                    error_msg = str(e)
                    
                    # Don't spam logs with timeout errors during fast polling
                    if "timeout" in error_msg.lower() or "VI_ERROR_TMO" in error_msg:
                        # Only log every 20th timeout to reduce spam
                        if self.error_count % 20 == 1:
                            logger.warning(f"Read timeout (x{self.error_count}): {error_msg}")
                    else:
                        logger.error(f"Read measurement failed: {error_msg}")
                    
                    return {"status": "error", "message": error_msg}
                
            elif cmd_type == 'setup_voltage_source':
                # Setup as voltage source
                voltage = cmd_data.get('voltage', 0)
                range_val = cmd_data.get('range', 'AUTO')
                compliance = cmd_data.get('compliance', 0.1)
                
                self.instrument.write(':SOUR:FUNC VOLT')
                self.instrument.write(':SOUR:VOLT:MODE FIXED')
                self.instrument.write(f':SOUR:VOLT {voltage}')
                if range_val != 'AUTO':
                    self.instrument.write(f':SOUR:VOLT:RANG {range_val}')
                self.instrument.write(':SENS:FUNC "CURR"')
                self.instrument.write(f':SENS:CURR:PROT {compliance}')
                
                return {"status": "success", "message": f"Voltage source setup: {voltage}V, compliance: {compliance}A"}
                
            elif cmd_type == 'setup_current_source':
                # Setup as current source
                current = cmd_data.get('current', 0)
                range_val = cmd_data.get('range', 'AUTO')
                compliance = cmd_data.get('compliance', 10)
                
                self.instrument.write(':SOUR:FUNC CURR')
                self.instrument.write(':SOUR:CURR:MODE FIXED')
                self.instrument.write(f':SOUR:CURR {current}')
                if range_val != 'AUTO':
                    self.instrument.write(f':SOUR:CURR:RANG {range_val}')
                self.instrument.write(':SENS:FUNC "VOLT"')
                self.instrument.write(f':SENS:VOLT:PROT {compliance}')
                
                return {"status": "success", "message": f"Current source setup: {current}A, compliance: {compliance}V"}
                
            elif cmd_type == 'output':
                # Control output
                state = cmd_data.get('state', 'OFF')
                self.instrument.write(f':OUTP {state}')
                return {"status": "success", "message": f"Output {state}"}
                
            elif cmd_type == 'voltage_sweep':
                # Perform voltage sweep
                start_v = cmd_data.get('start', 0)
                stop_v = cmd_data.get('stop', 5)
                steps = cmd_data.get('steps', 11)
                compliance = cmd_data.get('compliance', 0.1)
                delay = cmd_data.get('delay', 0.1)
                
                # Setup
                self.instrument.write(':SOUR:FUNC VOLT')
                self.instrument.write(':SOUR:VOLT:MODE FIXED')
                self.instrument.write(':SENS:FUNC "CURR"')
                self.instrument.write(f':SENS:CURR:PROT {compliance}')
                self.instrument.write(':OUTP ON')
                
                # Perform sweep
                voltages = [start_v + i * (stop_v - start_v) / (steps - 1) for i in range(steps)]
                results = []
                
                for voltage in voltages:
                    self.instrument.write(f':SOUR:VOLT {voltage}')
                    time.sleep(delay)
                    
                    reading = self.instrument.query(':READ?').strip()
                    values = reading.split(',')
                    
                    results.append({
                        "set_voltage": voltage,
                        "measured_voltage": float(values[0]),
                        "measured_current": float(values[1]),
                        "timestamp": datetime.now().isoformat()
                    })
                
                self.instrument.write(':OUTP OFF')
                return {"status": "success", "data": results}
                
            elif cmd_type == 'get_status':
                # Get instrument status
                idn = self.instrument.query('*IDN?').strip()
                output_state = self.instrument.query(':OUTP?').strip()
                source_func = self.instrument.query(':SOUR:FUNC?').strip()
                
                status = {
                    "instrument": idn,
                    "output": "ON" if output_state == "1" else "OFF",
                    "source_function": source_func,
                    "timestamp": datetime.now().isoformat(),
                    "fast_mode": self.fast_mode,
                    "read_count": self.read_count,
                    "error_count": self.error_count
                }
                return {"status": "success", "data": status}
                
            elif cmd_type == 'reset':
                # Reset instrument
                self.instrument.write('*RST')
                self.instrument.write('*CLS')
                
                # Re-apply fast mode settings after reset
                if self.fast_mode:
                    time.sleep(0.1)  # Wait for reset to complete
                    self.instrument.write(':SYST:AZER OFF')
                    self.instrument.write(':DISP:ENAB OFF')
                    self.instrument.write(':SENS:FUNC:CONC ON')
                    self.instrument.write(':FORM:ELEM VOLT,CURR')
                
                return {"status": "success", "message": "Instrument reset"}
                
            elif cmd_type == 'set_fast_mode':
                # Enable/disable fast mode
                self.fast_mode = cmd_data.get('enabled', True)
                if self.fast_mode:
                    self.instrument.write(':SYST:AZER OFF')
                    self.instrument.write(':DISP:ENAB OFF')
                    self.instrument.write(':SENS:FUNC:CONC ON')
                    self.instrument.write(':FORM:ELEM VOLT,CURR')
                else:
                    self.instrument.write(':SYST:AZER ON')
                    self.instrument.write(':DISP:ENAB ON')
                
                return {"status": "success", "message": f"Fast mode {'enabled' if self.fast_mode else 'disabled'}"}
                
            else:
                return {"status": "error", "message": f"Unknown command type: {cmd_type}"}
                
        except Exception as e:
            return {"status": "error", "message": str(e)}
            
    def cleanup(self):
        """Cleanup resources"""
        self.running = False
        
        # Close all client connections
        for client in self.clients:
            try:
                client.close()
            except:
                pass
                
        # Close server socket
        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
                
        # Close instrument connection
        if self.instrument:
            try:
                self.instrument.write(':OUTP OFF')  # Safety: turn output off
                if self.fast_mode:
                    # Restore normal settings
                    self.instrument.write(':SYST:AZER ON')
                    self.instrument.write(':DISP:ENAB ON')
                self.instrument.close()
            except:
                pass
                
        logger.info("Server cleanup completed")
        
    def stop_server(self):
        """Stop the server"""
        self.running = False
        if self.server_socket:
            self.server_socket.close()

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Keithley 2400 Server - Optimized for High-Rate Polling')
    parser.add_argument('--host', default='127.0.0.101', help='Server host')
    parser.add_argument('--port', type=int, default=8888, help='Server port')
    parser.add_argument('--gpib', default='GPIB1::24::INSTR', help='GPIB address')
    parser.add_argument('--fast', action='store_true', default=True, help='Enable fast mode (default: True)')
    parser.add_argument('--no-fast', action='store_true', help='Disable fast mode')
    
    args = parser.parse_args()
    
    # Handle fast mode flags
    fast_mode = args.fast and not args.no_fast
    
    server = Keithley2400Server(args.host, args.port, args.gpib, fast_mode)
    
    try:
        server.start_server()
    except KeyboardInterrupt:
        logger.info("Server interrupted by user")
    finally:
        server.cleanup()

if __name__ == "__main__":
    main()