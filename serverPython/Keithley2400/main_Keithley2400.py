import pyvisa
import tkinter as tk
from tkinter import ttk, messagebox
import threading
import time
import socket
import select
import queue
import traceback

class Keithley2400Server:
    def __init__(self, root):
        self.root = root
        self.root.title("Keithley 2400 Current Monitor - TCP Server")
        self.root.geometry("450x480")
        
        # Thread-safe variables
        self.collecting = False
        self.collecting_lock = threading.Lock()
        
        self.smu = None
        self.smu_lock = threading.Lock()
        
        # Voltage setting
        self.voltage_value = 3.0  # Default 3V
        
        # Rate calculation variables
        self.read_count = 0
        self.rate_start_time = 0
        self.rate_lock = threading.Lock()
        
        # TCP Server settings
        self.host = "127.0.0.55"
        self.port = 55555
        self.server_socket = None
        self.client_sockets = []
        self.clients_lock = threading.Lock()
        self.server_running = False
        
        # Thread-safe message queue for UI updates
        self.ui_queue = queue.Queue()
        
        # Error tracking
        self.consecutive_errors = 0
        self.max_errors = 10
        
        # Thread tracking
        self.read_thread = None
        
        # Setup UI
        self.setup_ui()
        
        # Connect to instrument
        self.connect_instrument()
        
        # Start TCP server
        self.start_tcp_server()
        
        # Start UI update loop
        self.process_ui_queue()
    
    def setup_ui(self):
        # Device ID display
        tk.Label(self.root, text="Device ID:", font=('Arial', 10, 'bold')).pack(pady=5)
        self.id_label = tk.Label(self.root, text="Not connected", wraplength=350)
        self.id_label.pack(pady=5)
        
        # TCP Server status
        server_frame = tk.Frame(self.root)
        server_frame.pack(pady=5)
        tk.Label(server_frame, text="TCP Server:", font=('Arial', 10, 'bold')).pack(side='left')
        self.server_status_label = tk.Label(server_frame, text=f" {self.host}:{self.port}", font=('Arial', 9), fg='blue')
        self.server_status_label.pack(side='left')
        self.client_count_label = tk.Label(server_frame, text=" (0 clients)", font=('Arial', 9), fg='gray')
        self.client_count_label.pack(side='left')
        
        # Voltage control frame
        voltage_frame = tk.Frame(self.root)
        voltage_frame.pack(pady=10)
        
        tk.Label(voltage_frame, text="Set Voltage (V):", font=('Arial', 10, 'bold')).pack(side='left', padx=5)
        
        self.voltage_entry = tk.Entry(voltage_frame, width=8, font=('Arial', 10))
        self.voltage_entry.insert(0, str(self.voltage_value))
        self.voltage_entry.pack(side='left', padx=5)
        
        self.set_voltage_btn = tk.Button(voltage_frame, text="Set", 
                                         command=self.set_voltage, bg='blue', fg='white')
        self.set_voltage_btn.pack(side='left', padx=5)
        
        # Settings display
        settings_frame = tk.Frame(self.root)
        settings_frame.pack(pady=5)
        self.settings_label = tk.Label(settings_frame, 
                                      text=f"V-Source: {self.voltage_value}V | I-Compliance: 500mA", 
                                      font=('Arial', 9), fg='blue')
        self.settings_label.pack()
        
        # Current display with unit selection
        current_frame = tk.Frame(self.root)
        current_frame.pack(pady=(20,5))
        
        tk.Label(current_frame, text="Current Reading:", font=('Arial', 12, 'bold')).pack()
        
        # Display frame for current value
        display_frame = tk.Frame(self.root, bg='white', relief='sunken')
        display_frame.pack(pady=5, padx=20, fill='x')
        
        self.current_label = tk.Label(display_frame, text="0.000", font=('Arial', 16), bg='white')
        self.current_label.pack(side='left', padx=(10,5))
        
        self.unit_label = tk.Label(display_frame, text="pA", font=('Arial', 12), bg='white')
        self.unit_label.pack(side='left', padx=(0,10))
        
        # Data rate display
        rate_frame = tk.Frame(self.root)
        rate_frame.pack(pady=10)
        tk.Label(rate_frame, text="Data Rate:", font=('Arial', 10)).pack(side='left')
        self.rate_label = tk.Label(rate_frame, text="0.0 Hz", font=('Arial', 10, 'bold'), fg='green')
        self.rate_label.pack(side='left', padx=5)
        self.rate_status = tk.Label(rate_frame, text="(Not collecting)", font=('Arial', 9), fg='gray')
        self.rate_status.pack(side='left')
        
        # Error display
        error_frame = tk.Frame(self.root)
        error_frame.pack(pady=5)
        tk.Label(error_frame, text="Errors:", font=('Arial', 9)).pack(side='left')
        self.error_count_label = tk.Label(error_frame, text="0", font=('Arial', 9, 'bold'), fg='green')
        self.error_count_label.pack(side='left', padx=5)
        
        # Control buttons
        button_frame = tk.Frame(self.root)
        button_frame.pack(pady=20)
        
        self.start_btn = tk.Button(button_frame, text="Start Collection", 
                                  command=self.start_collection, bg='green', fg='white')
        self.start_btn.pack(side='left', padx=10)
        
        self.stop_btn = tk.Button(button_frame, text="Stop Collection", 
                                 command=self.stop_collection, bg='red', fg='white', state='disabled')
        self.stop_btn.pack(side='left', padx=10)
        
        # Reconnect button
        self.reconnect_btn = tk.Button(button_frame, text="Reconnect", 
                                       command=self.reconnect_instrument, bg='orange', fg='white')
        self.reconnect_btn.pack(side='left', padx=10)
        
        # Status
        self.status_label = tk.Label(self.root, text="Ready", relief='sunken')
        self.status_label.pack(side='bottom', fill='x')
    
    def set_voltage(self):
        """Set the voltage value"""
        try:
            # Get and validate voltage value
            new_voltage = float(self.voltage_entry.get())
            
            # Check voltage limits (adjust as needed for your Keithley model)
            if new_voltage < -210 or new_voltage > 210:
                messagebox.showerror("Error", "Voltage must be between -210V and 210V")
                return
            
            self.voltage_value = new_voltage
            
            # Update display
            self.settings_label.config(text=f"V-Source: {self.voltage_value}V | I-Compliance: 500mA")
            
            # If collection is active, update the instrument
            with self.collecting_lock:
                if self.collecting:
                    with self.smu_lock:
                        if self.smu:
                            self.smu.write(f':SOUR:VOLT:LEV {self.voltage_value}')
                            self.queue_ui_update(lambda: self.status_label.config(
                                text=f"Collecting - V={self.voltage_value}V, I-Limit=500mA (Broadcasting)"))
            
            self.queue_ui_update(lambda: messagebox.showinfo("Success", 
                                                            f"Voltage set to {self.voltage_value}V"))
            
        except ValueError:
            messagebox.showerror("Error", "Please enter a valid number for voltage")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to set voltage: {str(e)}")
    
    def format_current(self, current_amps):
        """Format current value with appropriate unit (A, mA, μA, nA, pA)"""
        abs_current = abs(current_amps)
        
        if abs_current >= 1:
            return f"{current_amps:.3f}", "A"
        elif abs_current >= 1e-3:
            return f"{current_amps*1e3:.3f}", "mA"
        elif abs_current >= 1e-6:
            return f"{current_amps*1e6:.3f}", "μA"
        elif abs_current >= 1e-9:
            return f"{current_amps*1e9:.3f}", "nA"
        else:
            return f"{current_amps*1e12:.3f}", "pA"
    
    def queue_ui_update(self, func, *args):
        """Thread-safe UI update via queue"""
        self.ui_queue.put((func, args))
    
    def process_ui_queue(self):
        """Process queued UI updates"""
        try:
            while True:
                try:
                    func, args = self.ui_queue.get_nowait()
                    func(*args)
                except queue.Empty:
                    break
        except Exception as e:
            print(f"UI update error: {e}")
        finally:
            # Schedule next update
            self.root.after(50, self.process_ui_queue)
    
    def start_tcp_server(self):
        """Start TCP server thread"""
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(5)
            self.server_socket.setblocking(False)
            
            self.server_running = True
            self.server_thread = threading.Thread(target=self.tcp_server_thread, daemon=True)
            self.server_thread.start()
            
            self.queue_ui_update(lambda: self.server_status_label.config(
                text=f" {self.host}:{self.port} (Listening)", fg='green'))
            
        except Exception as e:
            self.queue_ui_update(lambda e=e: self.server_status_label.config(
                text=f" Server Error: {str(e)}", fg='red'))
    
    def tcp_server_thread(self):
        """Handle client connections with robust error handling"""
        while self.server_running:
            try:
                # Check for new connections and data
                readable, _, exceptional = select.select(
                    [self.server_socket] + self.client_sockets, 
                    [], 
                    self.client_sockets, 
                    0.1
                )
                
                for sock in readable:
                    if sock == self.server_socket:
                        # Accept new connection
                        try:
                            client_socket, address = self.server_socket.accept()
                            client_socket.setblocking(False)
                            
                            with self.clients_lock:
                                self.client_sockets.append(client_socket)
                                client_count = len(self.client_sockets)
                            
                            print(f"[DEBUG] Client connected from {address}. Total clients: {client_count}")
                            
                            # Check collection status
                            with self.collecting_lock:
                                if self.collecting:
                                    print(f"[DEBUG] New client connected during active collection")
                            
                            self.update_client_count()
                            
                        except socket.error as e:
                            print(f"Accept error: {e}")
                    else:
                        # Check if client sent data (or disconnected)
                        try:
                            data = sock.recv(1024)
                            if not data:
                                # Client disconnected
                                self.remove_client(sock)
                        except socket.error:
                            # Read error - client likely disconnected
                            self.remove_client(sock)
                
                # Handle exceptional conditions
                for sock in exceptional:
                    self.remove_client(sock)
                    
            except Exception as e:
                print(f"Server thread error: {e}")
                traceback.print_exc()
                
            time.sleep(0.01)
    
    def remove_client(self, sock):
        """Safely remove a client socket"""
        with self.clients_lock:
            if sock in self.client_sockets:
                try:
                    self.client_sockets.remove(sock)
                    sock.close()
                    print(f"[DEBUG] Client disconnected. Remaining clients: {len(self.client_sockets) - 1}")
                    self.update_client_count()
                    
                    # Check collection status
                    with self.collecting_lock:
                        if self.collecting:
                            print(f"[DEBUG] Collection still active after client disconnect")
                except Exception as e:
                    print(f"[DEBUG] Error removing client: {e}")
    
    def update_client_count(self):
        """Update client count display (thread-safe)"""
        with self.clients_lock:
            count = len(self.client_sockets)
        
        self.queue_ui_update(lambda count=count: self.client_count_label.config(
            text=f" ({count} client{'s' if count != 1 else ''})", 
            fg='green' if count > 0 else 'gray'
        ))
    
    def broadcast_value(self, current_amps):
        """Broadcast current value to all connected clients (thread-safe)"""
        with self.clients_lock:
            if not self.client_sockets:
                return
            
            # Format as string with newline for easy parsing
            message = f"{current_amps}\n".encode('utf-8')
            
            # Send to all clients
            disconnected = []
            for client in self.client_sockets[:]:  # Copy list to iterate safely
                try:
                    client.send(message)
                except (socket.error, BrokenPipeError, ConnectionResetError):
                    # Client disconnected
                    disconnected.append(client)
            
            # Remove disconnected clients
            for client in disconnected:
                if client in self.client_sockets:
                    self.client_sockets.remove(client)
                    try:
                        client.close()
                    except:
                        pass
        
        if disconnected:
            self.update_client_count()
    
    def connect_instrument(self):
        """Connect to Keithley with error handling"""
        try:
            with self.smu_lock:
                rm = pyvisa.ResourceManager()
                self.smu = rm.open_resource('GPIB1::24::INSTR')
                self.smu.timeout = 5000  # 5 second timeout
                
                # Test connection
                device_id = self.smu.query('*IDN?').strip()
                
                # Basic reset
                self.smu.write('*RST')
                
            self.queue_ui_update(lambda id=device_id: self.id_label.config(text=id))
            self.queue_ui_update(lambda: self.status_label.config(text="Connected - Ready"))
            self.consecutive_errors = 0
            
        except Exception as e:
            self.queue_ui_update(lambda e=e: (
                self.status_label.config(text=f"Connection Error: {str(e)}", fg='red'),
                self.id_label.config(text="Not connected")
            ))
            with self.smu_lock:
                self.smu = None
    
    def reconnect_instrument(self):
        """Reconnect to instrument"""
        # Stop collection if running
        if self.collecting:
            self.stop_collection()
        
        # Reconnect
        self.queue_ui_update(lambda: self.status_label.config(text="Reconnecting..."))
        self.connect_instrument()
    
    def reset_ui_state(self):
        """Reset UI to proper state after collection stops"""
        self.queue_ui_update(lambda: (
            self.start_btn.config(state='normal'),
            self.stop_btn.config(state='disabled'),
            self.status_label.config(text="Stopped - Output OFF"),
            self.rate_label.config(text="0.0 Hz"),
            self.rate_status.config(text="(Not collecting)", fg='gray'),
            self.error_count_label.config(text="0", fg='green')
        ))
    
    def start_collection(self):
        """Start data collection with error handling"""
        print(f"[DEBUG] Start collection requested")
        
        # First ensure any previous collection is fully stopped
        with self.collecting_lock:
            if self.collecting:
                print("[DEBUG] Collection already running, ignoring start request")
                return
        
        # Wait for previous thread to finish if it exists
        if self.read_thread and self.read_thread.is_alive():
            print("[DEBUG] Previous read thread still alive, waiting for it to finish...")
            self.read_thread.join(timeout=2.0)
            if self.read_thread.is_alive():
                print("[DEBUG] ERROR: Previous thread still running after timeout, cannot start new collection")
                self.queue_ui_update(lambda: messagebox.showerror(
                    "Error", "Previous collection still active. Please wait and try again."))
                return
            else:
                print("[DEBUG] Previous thread finished successfully")
        
        with self.smu_lock:
            if not self.smu:
                print("[DEBUG] ERROR: SMU not connected")
                self.queue_ui_update(lambda: messagebox.showerror(
                    "Error", "Keithley not connected. Please reconnect."))
                return
        
        print(f"[DEBUG] Configuring instrument for collection...")
        
        try:
            with self.smu_lock:
                # Configure for voltage source, current measurement
                self.smu.write(':SOUR:FUNC VOLT')              # Set to voltage source
                self.smu.write(':SOUR:VOLT:MODE FIXED')        # Fixed voltage mode  
                self.smu.write(f':SOUR:VOLT:LEV {self.voltage_value}')  # Set voltage
                self.smu.write(':SENS:FUNC "CURR"')            # Measure current
                self.smu.write(':SENS:CURR:PROT 0.5')          # Current compliance = 500mA
                self.smu.write(':SENS:CURR:RANG:AUTO ON')      # Auto range for current measurement
                
                # Optimize for speed
                self.smu.write(':SENS:CURR:NPLC 0.01')         # Fastest integration time
                self.smu.write(':DISP:ENAB OFF')               # Turn off display for speed
                self.smu.write(':FORM:ELEM CURR')              # Return only current value
                
                # Turn output ON
                self.smu.write(':OUTP ON')
            
            with self.collecting_lock:
                self.collecting = True
            
            with self.rate_lock:
                self.read_count = 0
                self.rate_start_time = time.time()
            
            self.consecutive_errors = 0
            
            self.queue_ui_update(lambda: (
                self.start_btn.config(state='disabled'),
                self.stop_btn.config(state='normal'),
                self.status_label.config(text=f"Collecting - V={self.voltage_value}V, I-Limit=500mA (Broadcasting)"),
                self.rate_status.config(text="(Active)", fg='green'),
                self.set_voltage_btn.config(state='normal')  # Allow voltage changes during collection
            ))
            
            # Start reading thread
            self.read_thread = threading.Thread(target=self.read_current_loop, daemon=True)
            self.read_thread.start()
            print(f"[DEBUG] Collection started successfully. Thread ID: {self.read_thread.ident}")
            
        except Exception as e:
            print(f"[DEBUG] ERROR starting collection: {e}")
            self.queue_ui_update(lambda e=e: (
                self.status_label.config(text=f"Start Error: {str(e)}", fg='red'),
                messagebox.showerror("Error", f"Failed to start collection: {str(e)}")
            ))
            with self.collecting_lock:
                self.collecting = False
            self.reset_ui_state()
    
    def stop_collection(self):
        """Stop data collection safely"""
        print(f"[DEBUG] Stop collection requested")
        
        # Set flag to stop collection
        with self.collecting_lock:
            was_collecting = self.collecting
            self.collecting = False
            print(f"[DEBUG] Collection flag set to False (was: {was_collecting})")
        
        if not was_collecting:
            print("[DEBUG] Collection was not running, just resetting UI")
            self.reset_ui_state()
            return
        
        # Wait for thread to finish with timeout
        if self.read_thread and self.read_thread.is_alive():
            thread_id = self.read_thread.ident
            print(f"[DEBUG] Waiting for read thread {thread_id} to stop...")
            self.read_thread.join(timeout=2.0)
            if self.read_thread.is_alive():
                print(f"[DEBUG] WARNING: Read thread {thread_id} did not stop in time!")
            else:
                print(f"[DEBUG] Read thread {thread_id} stopped successfully")
        else:
            print("[DEBUG] Read thread was not alive")
        
        # Stop instrument output
        print("[DEBUG] Turning off instrument output...")
        try:
            with self.smu_lock:
                if self.smu:
                    self.smu.write(':OUTP OFF')
                    self.smu.write(':SOUR:VOLT:LEV 0')  # Set voltage back to 0V
                    print("[DEBUG] Instrument output turned OFF successfully")
        except Exception as e:
            print(f"[DEBUG] ERROR stopping instrument: {e}")
        
        # Reset UI state
        print("[DEBUG] Resetting UI state")
        self.reset_ui_state()
        print("[DEBUG] Stop collection completed")
    
    def update_rate_display(self):
        """Calculate and display the data acquisition rate (thread-safe)"""
        with self.rate_lock:
            current_time = time.time()
            elapsed = current_time - self.rate_start_time
            
            if elapsed >= 1.0:  # Update rate every second
                rate = self.read_count / elapsed
                self.queue_ui_update(lambda r=rate: self.rate_label.config(text=f"{r:.1f} Hz"))
                self.read_count = 0
                self.rate_start_time = current_time
    
    def read_current_loop(self):
        """Read current values with comprehensive error handling"""
        thread_id = threading.current_thread().ident
        print(f"[DEBUG] Read thread {thread_id} started")
        
        try:
            loop_count = 0
            while True:
                # Check if we should stop
                with self.collecting_lock:
                    if not self.collecting:
                        print(f"[DEBUG] Read thread {thread_id}: Collection flag is False, stopping read loop")
                        break
                
                loop_count += 1
                if loop_count % 1000 == 0:  # Log every 1000 iterations
                    with self.clients_lock:
                        client_count = len(self.client_sockets)
                    print(f"[DEBUG] Read thread {thread_id}: Loop iteration {loop_count}, {client_count} clients connected")
                
                try:
                    with self.smu_lock:
                        if not self.smu:
                            raise Exception("Instrument disconnected")
                        
                        # Read measurement
                        reading = self.smu.query(':READ?')
                    
                    # Parse reading
                    try:
                        current = float(reading.strip())
                    except:
                        # If still getting full format, parse it
                        values = reading.strip().split(',')
                        current = float(values[1] if len(values) > 1 else values[0])
                    
                    # Reset error counter on successful read
                    self.consecutive_errors = 0
                    self.queue_ui_update(lambda: self.error_count_label.config(text="0", fg='green'))
                    
                    # Broadcast to TCP clients (with error handling)
                    try:
                        self.broadcast_value(current)
                    except Exception as e:
                        print(f"[DEBUG] Broadcast error (non-fatal): {e}")
                        # Don't stop collection on broadcast errors
                    
                    # Format for display
                    value_str, unit = self.format_current(current)
                    
                    # Update UI
                    self.queue_ui_update(lambda v=value_str, u=unit: (
                        self.current_label.config(text=v),
                        self.unit_label.config(text=u)
                    ))
                    
                    # Update rate
                    with self.rate_lock:
                        self.read_count += 1
                    self.update_rate_display()
                    
                    time.sleep(0.001)  # 1ms delay for ~1000Hz max
                    
                except pyvisa.errors.VisaIOError as e:
                    # VISA communication error
                    self.consecutive_errors += 1
                    self.queue_ui_update(lambda c=self.consecutive_errors: 
                        self.error_count_label.config(text=str(c), fg='red'))
                    
                    if self.consecutive_errors >= self.max_errors:
                        error_msg = f"Too many VISA errors ({self.consecutive_errors}). Stopping collection."
                        print(f"[DEBUG] {error_msg}")
                        print(f"[DEBUG] VISA Error details: {e}")
                        self.queue_ui_update(lambda msg=error_msg: (
                            self.status_label.config(text=msg, fg='red'),
                            messagebox.showerror("Communication Error", 
                                "Lost connection to Keithley. Please check connection and restart.")
                        ))
                        break
                    
                    time.sleep(0.1)  # Brief pause before retry
                    
                except Exception as e:
                    # Other errors
                    if self.consecutive_errors == 0:  # Only log first occurrence
                        print(f"[DEBUG] Read error (attempt {self.consecutive_errors + 1}/{self.max_errors}): {e}")
                        traceback.print_exc()
                    self.consecutive_errors += 1
                    
                    if self.consecutive_errors >= self.max_errors:
                        print(f"[DEBUG] Too many read errors ({self.consecutive_errors}), stopping collection")
                        self.queue_ui_update(lambda: self.status_label.config(
                            text=f"Read Error: {str(e)}", fg='red'))
                        break
                    
                    time.sleep(0.1)
        finally:
            # Ensure clean exit
            print(f"[DEBUG] Read thread {thread_id} exiting...")
            with self.collecting_lock:
                self.collecting = False
                print(f"[DEBUG] Read thread {thread_id}: Collection flag set to False on exit")
            
            # Ensure UI is reset
            print(f"[DEBUG] Read thread {thread_id}: Requesting UI reset")
            self.queue_ui_update(self.reset_ui_state)
            print(f"[DEBUG] Read thread {thread_id} fully terminated")
    
    def cleanup(self):
        """Clean up resources on exit"""
        print("Cleaning up...")
        self.server_running = False
        
        # Stop collection
        with self.collecting_lock:
            self.collecting = False
        
        # Wait for read thread to finish
        if self.read_thread and self.read_thread.is_alive():
            self.read_thread.join(timeout=2.0)
        
        # Close all client connections
        with self.clients_lock:
            for client in self.client_sockets:
                try:
                    client.close()
                except:
                    pass
            self.client_sockets.clear()
        
        # Close server socket
        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
        
        # Close instrument connection
        with self.smu_lock:
            if self.smu:
                try:
                    self.smu.write(':OUTP OFF')
                    self.smu.close()
                except:
                    pass

if __name__ == "__main__":
    root = tk.Tk()
    app = Keithley2400Server(root)
    
    # Handle cleanup on window close
    def on_closing():
        app.cleanup()
        root.destroy()
    
    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()