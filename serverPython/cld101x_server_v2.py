import pyvisa
import socket
import threading
import tkinter as tk
from tkinter import ttk, scrolledtext
import time
import queue
from datetime import datetime

class CLD101xServerGUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("CLD101x Server Control Panel")
        self.root.geometry("800x600")
        
        # Server state
        self.server_running = False
        self.server_thread = None
        self.server_socket = None
        self.client_count = 0
        self.total_commands = 0
        
        # VISA instrument
        self.instr = None
        self.rm = None
        
        # Communication queues
        self.log_queue = queue.Queue()
        self.status_queue = queue.Queue()
        
        # Server settings
        self.host = tk.StringVar(value="127.0.0.11")
        self.port = tk.IntVar(value=65432)
        self.visa_resource = tk.StringVar(value="USB0::0x1313::0x804F::M00930341::INSTR")
        
        # Current readings
        self.current_temp = tk.StringVar(value="--")
        self.current_laser = tk.StringVar(value="--")
        self.laser_state = tk.StringVar(value="--")
        self.tec_state = tk.StringVar(value="--")
        
        self.setup_ui()
        self.start_log_updater()
        
    def setup_ui(self):
        # Main notebook for tabs
        notebook = ttk.Notebook(self.root)
        notebook.pack(fill='both', expand=True, padx=5, pady=5)
        
        # Server Control Tab
        server_frame = ttk.Frame(notebook)
        notebook.add(server_frame, text="Server Control")
        self.setup_server_tab(server_frame)
        
        # Instrument Status Tab
        status_frame = ttk.Frame(notebook)
        notebook.add(status_frame, text="Instrument Status")
        self.setup_status_tab(status_frame)
        
        # Logs Tab
        log_frame = ttk.Frame(notebook)
        notebook.add(log_frame, text="Activity Logs")
        self.setup_log_tab(log_frame)
        
    def setup_server_tab(self, parent):
        # Connection Settings
        settings_frame = ttk.LabelFrame(parent, text="Connection Settings")
        settings_frame.pack(fill='x', padx=5, pady=5)
        
        ttk.Label(settings_frame, text="Host:").grid(row=0, column=0, sticky='w', padx=5, pady=2)
        ttk.Entry(settings_frame, textvariable=self.host, width=15).grid(row=0, column=1, padx=5, pady=2)
        
        ttk.Label(settings_frame, text="Port:").grid(row=0, column=2, sticky='w', padx=5, pady=2)
        ttk.Entry(settings_frame, textvariable=self.port, width=8).grid(row=0, column=3, padx=5, pady=2)
        
        ttk.Label(settings_frame, text="VISA Resource:").grid(row=1, column=0, sticky='w', padx=5, pady=2)
        ttk.Entry(settings_frame, textvariable=self.visa_resource, width=40).grid(row=1, column=1, columnspan=3, sticky='ew', padx=5, pady=2)
        
        # Server Control
        control_frame = ttk.LabelFrame(parent, text="Server Control")
        control_frame.pack(fill='x', padx=5, pady=5)
        
        self.start_btn = ttk.Button(control_frame, text="Start Server", command=self.start_server)
        self.start_btn.pack(side='left', padx=5, pady=5)
        
        self.stop_btn = ttk.Button(control_frame, text="Stop Server", command=self.stop_server, state='disabled')
        self.stop_btn.pack(side='left', padx=5, pady=5)
        
        ttk.Button(control_frame, text="Test VISA Connection", command=self.test_visa).pack(side='left', padx=5, pady=5)
        
        # Server Status
        status_frame = ttk.LabelFrame(parent, text="Server Status")
        status_frame.pack(fill='both', expand=True, padx=5, pady=5)
        
        self.status_label = ttk.Label(status_frame, text="Server: Stopped", font=('Arial', 12, 'bold'))
        self.status_label.pack(pady=5)
        
        self.client_label = ttk.Label(status_frame, text="Active Clients: 0")
        self.client_label.pack(pady=2)
        
        self.command_label = ttk.Label(status_frame, text="Total Commands: 0")
        self.command_label.pack(pady=2)
        
        self.visa_label = ttk.Label(status_frame, text="VISA: Not Connected")
        self.visa_label.pack(pady=2)
        
    def setup_status_tab(self, parent):
        # Current Readings
        readings_frame = ttk.LabelFrame(parent, text="Current Readings")
        readings_frame.pack(fill='x', padx=5, pady=5)
        
        ttk.Label(readings_frame, text="Temperature:").grid(row=0, column=0, sticky='w', padx=5, pady=5)
        ttk.Label(readings_frame, textvariable=self.current_temp, font=('Arial', 12, 'bold')).grid(row=0, column=1, sticky='w', padx=5, pady=5)
        ttk.Label(readings_frame, text="°C").grid(row=0, column=2, sticky='w', padx=2, pady=5)
        
        ttk.Label(readings_frame, text="Laser Current:").grid(row=1, column=0, sticky='w', padx=5, pady=5)
        ttk.Label(readings_frame, textvariable=self.current_laser, font=('Arial', 12, 'bold')).grid(row=1, column=1, sticky='w', padx=5, pady=5)
        ttk.Label(readings_frame, text="A").grid(row=1, column=2, sticky='w', padx=2, pady=5)
        
        # States
        states_frame = ttk.LabelFrame(parent, text="Device States")
        states_frame.pack(fill='x', padx=5, pady=5)
        
        ttk.Label(states_frame, text="Laser:").grid(row=0, column=0, sticky='w', padx=5, pady=5)
        ttk.Label(states_frame, textvariable=self.laser_state, font=('Arial', 12, 'bold')).grid(row=0, column=1, sticky='w', padx=5, pady=5)
        
        ttk.Label(states_frame, text="TEC:").grid(row=1, column=0, sticky='w', padx=5, pady=5)
        ttk.Label(states_frame, textvariable=self.tec_state, font=('Arial', 12, 'bold')).grid(row=1, column=1, sticky='w', padx=5, pady=5)
        
        # Manual Control
        control_frame = ttk.LabelFrame(parent, text="Manual Control")
        control_frame.pack(fill='x', padx=5, pady=5)
        
        ttk.Button(control_frame, text="Read Temperature", command=self.read_temperature).pack(side='left', padx=5, pady=5)
        ttk.Button(control_frame, text="Read Current", command=self.read_current).pack(side='left', padx=5, pady=5)
        ttk.Button(control_frame, text="Update All", command=self.update_readings).pack(side='left', padx=5, pady=5)
        
    def setup_log_tab(self, parent):
        # Log display
        self.log_text = scrolledtext.ScrolledText(parent, height=20, width=80)
        self.log_text.pack(fill='both', expand=True, padx=5, pady=5)
        
        # Log controls
        control_frame = ttk.Frame(parent)
        control_frame.pack(fill='x', padx=5, pady=5)
        
        ttk.Button(control_frame, text="Clear Logs", command=self.clear_logs).pack(side='left', padx=5)
        
        self.auto_scroll = tk.BooleanVar(value=True)
        ttk.Checkbutton(control_frame, text="Auto-scroll", variable=self.auto_scroll).pack(side='left', padx=5)
        
    def log_message(self, message, level="INFO"):
        timestamp = datetime.now().strftime("%H:%M:%S")
        log_entry = f"[{timestamp}] {level}: {message}\n"
        self.log_queue.put(log_entry)
        
    def start_log_updater(self):
        """Update logs from queue in main thread"""
        try:
            while True:
                log_entry = self.log_queue.get_nowait()
                self.log_text.insert(tk.END, log_entry)
                if self.auto_scroll.get():
                    self.log_text.see(tk.END)
        except queue.Empty:
            pass
        
        # Schedule next update
        self.root.after(100, self.start_log_updater)
        
    def test_visa(self):
        try:
            if self.rm is None:
                self.rm = pyvisa.ResourceManager()
            
            if self.instr is not None:
                self.instr.close()
                
            self.instr = self.rm.open_resource(self.visa_resource.get())
            self.instr.timeout = 1000
            
            idn = self.instr.query("*IDN?")
            self.visa_label.config(text=f"VISA: Connected - {idn.strip()}")
            self.log_message(f"VISA connected: {idn.strip()}")
            
            # Update initial readings
            self.update_readings()
            
        except Exception as e:
            self.visa_label.config(text="VISA: Connection Failed")
            self.log_message(f"VISA connection failed: {e}", "ERROR")
            
    def start_server(self):
        if self.server_running:
            return
            
        try:
            # Test VISA connection first
            if self.instr is None:
                self.test_visa()
                if self.instr is None:
                    self.log_message("Cannot start server: VISA not connected", "ERROR")
                    return
            
            # Start server thread
            self.server_thread = threading.Thread(target=self.server_worker, daemon=True)
            self.server_running = True
            self.server_thread.start()
            
            self.start_btn.config(state='disabled')
            self.stop_btn.config(state='normal')
            self.status_label.config(text="Server: Running")
            
            self.log_message(f"Server started on {self.host.get()}:{self.port.get()}")
            
        except Exception as e:
            self.log_message(f"Failed to start server: {e}", "ERROR")
            
    def stop_server(self):
        if not self.server_running:
            return
            
        self.server_running = False
        
        if self.server_socket:
            self.server_socket.close()
            
        self.start_btn.config(state='normal')
        self.stop_btn.config(state='disabled')
        self.status_label.config(text="Server: Stopped")
        
        self.log_message("Server stopped")
        
    def server_worker(self):
        """Server worker thread"""
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind((self.host.get(), self.port.get()))
            self.server_socket.listen()
            
            while self.server_running:
                try:
                    self.server_socket.settimeout(1.0)  # Non-blocking accept
                    conn, addr = self.server_socket.accept()
                    
                    # Handle client in separate thread
                    client_thread = threading.Thread(target=self.handle_client, args=(conn, addr), daemon=True)
                    client_thread.start()
                    
                except socket.timeout:
                    continue
                except Exception as e:
                    if self.server_running:
                        self.log_message(f"Server error: {e}", "ERROR")
                    break
                    
        except Exception as e:
            self.log_message(f"Server startup error: {e}", "ERROR")
        finally:
            if self.server_socket:
                self.server_socket.close()
                
    def handle_client(self, conn, addr):
        """Handle individual client connection"""
        self.client_count += 1
        self.root.after(0, lambda: self.client_label.config(text=f"Active Clients: {self.client_count}"))
        self.log_message(f"Client connected: {addr}")
        
        try:
            while self.server_running:
                try:
                    conn.settimeout(1.0)
                    data = conn.recv(1024).decode()
                    if not data:
                        break
                        
                    response = self.process_command(data.strip())
                    conn.sendall(response.encode())
                    
                    self.total_commands += 1
                    self.root.after(0, lambda: self.command_label.config(text=f"Total Commands: {self.total_commands}"))
                    
                except socket.timeout:
                    continue
                except Exception as e:
                    self.log_message(f"Client {addr} error: {e}", "ERROR")
                    break
                    
        finally:
            conn.close()
            self.client_count -= 1
            self.root.after(0, lambda: self.client_label.config(text=f"Active Clients: {self.client_count}"))
            self.log_message(f"Client disconnected: {addr}")
            
    def process_command(self, data):
        """Process incoming command and return response"""
        self.log_message(f"Command: {data}")
        
        parts = data.split()
        command = parts[0] if parts else ""
        value = parts[1] if len(parts) > 1 else None
        
        try:
            if command == "SET_LASER_CURRENT" and value is not None:
                self.instr.write(f"source1:current:level:amplitude {value}")
                response = f"Set LD current [A]: {self.instr.query('source1:current:level:amplitude?')}"
                
            elif command == "SET_TEC_TEMPERATURE" and value is not None:
                self.instr.write(f"source2:temperature:spoint {value}")
                response = f"Set TEC temperature [C]: {self.instr.query('source2:temperature:spoint?')}"
                
            elif command == "LASER_ON":
                self.instr.write("output1:state on")
                response = f"Laser state: {self.instr.query('output1:state?')}"
                self.root.after(0, lambda: self.laser_state.set(response.split(': ')[1]))
                
            elif command == "LASER_OFF":
                self.instr.write("output1:state off")
                response = f"Laser state: {self.instr.query('output1:state?')}"
                self.root.after(0, lambda: self.laser_state.set(response.split(': ')[1]))
                
            elif command == "TEC_ON":
                self.instr.write("output2:state on")
                response = f"TEC state: {self.instr.query('output2:state?')}"
                self.root.after(0, lambda: self.tec_state.set(response.split(': ')[1]))
                
            elif command == "TEC_OFF":
                self.instr.write("output2:state off")
                response = f"TEC state: {self.instr.query('output2:state?')}"
                self.root.after(0, lambda: self.tec_state.set(response.split(': ')[1]))
                
            elif command == "READ_LASER_CURRENT":
                current = self.instr.query("sense3:current:dc:data?")
                response = f"Current laser current [A]: {current}"
                self.root.after(0, lambda: self.current_laser.set(current.strip()))
                
            elif command == "READ_TEC_TEMPERATURE":
                try:
                    original_timeout = self.instr.timeout
                    self.instr.timeout = 200
                    temp = self.instr.query("SENSe2:temperature:data?")
                    response = f"Current TEC temperature [C]: {temp}"
                    self.root.after(0, lambda: self.current_temp.set(temp.strip()))
                except pyvisa.errors.VisaIOError as e:
                    if "timeout" in str(e).lower():
                        response = "ERROR: Temperature reading timeout"
                    else:
                        response = f"ERROR: VISA error - {str(e)}"
                finally:
                    self.instr.timeout = original_timeout
                    
            else:
                response = "Unknown command or missing value"
                
        except pyvisa.errors.VisaIOError as e:
            response = f"ERROR: VISA communication error - {str(e)}"
            self.log_message(f"VISA error: {e}", "ERROR")
            
        except Exception as e:
            response = f"ERROR: {str(e)}"
            self.log_message(f"Command error: {e}", "ERROR")
            
        self.log_message(f"Response: {response}")
        return response
        
    def read_temperature(self):
        """Manual temperature reading"""
        if self.instr is None:
            self.log_message("VISA not connected", "ERROR")
            return
            
        try:
            original_timeout = self.instr.timeout
            self.instr.timeout = 200
            temp = self.instr.query("SENSe2:temperature:data?")
            self.current_temp.set(temp.strip())
            self.log_message(f"Temperature: {temp.strip()} °C")
        except Exception as e:
            self.log_message(f"Failed to read temperature: {e}", "ERROR")
        finally:
            self.instr.timeout = original_timeout
            
    def read_current(self):
        """Manual current reading"""
        if self.instr is None:
            self.log_message("VISA not connected", "ERROR")
            return
            
        try:
            current = self.instr.query("sense3:current:dc:data?")
            self.current_laser.set(current.strip())
            self.log_message(f"Current: {current.strip()} A")
        except Exception as e:
            self.log_message(f"Failed to read current: {e}", "ERROR")
            
    def update_readings(self):
        """Update all readings"""
        self.read_temperature()
        self.read_current()
        
        try:
            laser_state = self.instr.query("output1:state?")
            self.laser_state.set(laser_state.strip())
            
            tec_state = self.instr.query("output2:state?")
            self.tec_state.set(tec_state.strip())
        except Exception as e:
            self.log_message(f"Failed to read states: {e}", "ERROR")
            
    def clear_logs(self):
        """Clear log display"""
        self.log_text.delete(1.0, tk.END)
        
    def run(self):
        """Start the GUI application"""
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.root.mainloop()
        
    def on_closing(self):
        """Clean shutdown"""
        self.stop_server()
        
        if self.instr:
            try:
                self.instr.close()
            except:
                pass
                
        if self.rm:
            try:
                self.rm.close()
            except:
                pass
                
        self.root.destroy()

if __name__ == "__main__":
    app = CLD101xServerGUI()
    app.run()