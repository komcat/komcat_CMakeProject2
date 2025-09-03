"""Data collector optimized for alignment applications."""

import threading
import time
import numpy as np
from typing import Dict, Optional
from PyQt5.QtCore import QObject, pyqtSignal
from collections import deque
import queue


class AlignmentDataCollector(QObject):
    """Data collector for alignment with adaptive speed."""
    
    # Signals
    ch1_data_updated = pyqtSignal(float, str)  # value, range_info
    ch2_data_updated = pyqtSignal(float, str)
    ch1_histogram_updated = pyqtSignal(list)
    ch2_histogram_updated = pyqtSignal(list)
    ch1_client_count_changed = pyqtSignal(int)
    ch2_client_count_changed = pyqtSignal(int)
    
    # Signal quality indicators
    alignment_quality = pyqtSignal(int, float)  # channel, quality percentage
    signal_found = pyqtSignal(int)  # channel where signal found
    signal_lost = pyqtSignal(int)   # channel where signal lost
    
    def __init__(self, multimeter, parent=None):
        """Initialize alignment data collector."""
        super().__init__(parent)
        self.multimeter = multimeter
        self.running = False
        
        # Client management
        self.ch1_clients = []
        self.ch2_clients = []
        self.ch1_lock = threading.Lock()
        self.ch2_lock = threading.Lock()
        
        # Data storage
        self.ch1_buffer = deque(maxlen=1000)
        self.ch2_buffer = deque(maxlen=1000)
        
        # Peak tracking for alignment
        self.ch1_peak = 0
        self.ch2_peak = 0
        self.ch1_baseline = 0
        self.ch2_baseline = 0
        
        # Statistics
        self.ch1_data_count = 0
        self.ch2_data_count = 0
        
        # Timing
        self.ch1_elapsed_times = deque(maxlen=100)
        self.ch2_elapsed_times = deque(maxlen=100)
        
        # Alignment mode
        self.alignment_mode = "COARSE"
        self.signal_threshold = 1e-9  # 1nA threshold for "signal found"
        
    def start(self):
        """Start data collection."""
        self.running = True
        
        # Main collection thread
        data_thread = threading.Thread(target=self._collect_data, daemon=True)
        data_thread.start()
        
        # Client broadcast threads
        for ch in [1, 2]:
            thread = threading.Thread(
                target=lambda c=ch: self._broadcast_thread(c), 
                daemon=True
            )
            thread.start()
        
        # Analysis thread for alignment quality
        analysis_thread = threading.Thread(target=self._analyze_alignment, daemon=True)
        analysis_thread.start()
        
        # Histogram threads
        for ch in [1, 2]:
            thread = threading.Thread(
                target=lambda c=ch: self._calculate_histogram(c),
                daemon=True
            )
            thread.start()
    
    def stop(self):
        """Stop data collection."""
        self.running = False
    
    def set_mode(self, mode: str):
        """
        Switch alignment mode.
        
        Args:
            mode: "SEARCH", "COARSE", "FINE", or "MONITOR"
        """
        self.alignment_mode = mode
        
        # Configure multimeter based on mode
        if mode == "SEARCH":
            self.multimeter.configure_for_alignment("LOW")
            self.signal_threshold = 1e-12  # 1pA for search mode
        elif mode == "COARSE":
            self.multimeter.configure_for_alignment("MID")
            self.signal_threshold = 1e-9   # 1nA
        elif mode == "FINE":
            self.multimeter.configure_for_alignment("AUTO")
            self.signal_threshold = 1e-6   # 1uA
        elif mode == "MONITOR":
            self.multimeter.configure_fast_mode()
            self.signal_threshold = 1e-3   # 1mA
    
    def _collect_data(self):
        """Main data collection loop with adaptive reading."""
        while self.running:
            try:
                start_time = time.perf_counter()
                
                # Read based on mode
                if self.alignment_mode == "MONITOR":
                    # Fast mode for monitoring
                    measurements = self.multimeter.get_fast_measurement()
                    range_info = {1: "20mA", 2: "20mA"}
                else:
                    # Smart ranging for alignment
                    raw_data = self.multimeter.read_with_smart_ranging()
                    measurements = {}
                    range_info = {}
                    for ch, (value, info) in raw_data.items():
                        measurements[ch] = value
                        range_info[ch] = info
                
                elapsed = time.perf_counter() - start_time
                
                # Process channel 1
                if 1 in measurements and measurements[1] is not None:
                    value = measurements[1]
                    self.ch1_buffer.append(value)
                    self.ch1_data_count += 1
                    self.ch1_elapsed_times.append(elapsed)
                    
                    # Track peak for alignment
                    if abs(value) > self.ch1_peak:
                        self.ch1_peak = abs(value)
                    
                    # Check signal status
                    if abs(value) > self.signal_threshold:
                        if self.ch1_baseline == 0:  # First signal detection
                            self.signal_found.emit(1)
                        self.ch1_baseline = value
                    elif self.ch1_baseline != 0:
                        self.signal_lost.emit(1)
                        self.ch1_baseline = 0
                    
                    # Emit with range info
                    self.ch1_data_updated.emit(value, range_info.get(1, ""))
                
                # Process channel 2
                if 2 in measurements and measurements[2] is not None:
                    value = measurements[2]
                    self.ch2_buffer.append(value)
                    self.ch2_data_count += 1
                    self.ch2_elapsed_times.append(elapsed)
                    
                    # Track peak
                    if abs(value) > self.ch2_peak:
                        self.ch2_peak = abs(value)
                    
                    # Check signal status
                    if abs(value) > self.signal_threshold:
                        if self.ch2_baseline == 0:
                            self.signal_found.emit(2)
                        self.ch2_baseline = value
                    elif self.ch2_baseline != 0:
                        self.signal_lost.emit(2)
                        self.ch2_baseline = 0
                    
                    self.ch2_data_updated.emit(value, range_info.get(2, ""))
                
                # Adaptive delay based on mode
                if self.alignment_mode == "SEARCH":
                    time.sleep(0.1)   # Slower for search
                elif self.alignment_mode == "MONITOR":
                    time.sleep(0.001)  # Fastest for monitoring
                else:
                    time.sleep(0.01)   # Medium for alignment
                    
            except Exception as e:
                print(f"Collection error: {e}")
                time.sleep(0.1)
    
    def _analyze_alignment(self):
        """Analyze alignment quality."""
        while self.running:
            time.sleep(1)  # Analyze every second
            
            # Calculate alignment quality for each channel
            if len(self.ch1_buffer) > 10:
                recent = list(self.ch1_buffer)[-50:]
                if self.ch1_peak > 0:
                    current_avg = np.mean(np.abs(recent))
                    quality = (current_avg / self.ch1_peak) * 100
                    self.alignment_quality.emit(1, min(quality, 100))
            
            if len(self.ch2_buffer) > 10:
                recent = list(self.ch2_buffer)[-50:]
                if self.ch2_peak > 0:
                    current_avg = np.mean(np.abs(recent))
                    quality = (current_avg / self.ch2_peak) * 100
                    self.alignment_quality.emit(2, min(quality, 100))
    
    def _broadcast_thread(self, channel: int):
        """Broadcast data to clients."""
        if channel == 1:
            buffer = self.ch1_buffer
            lock = self.ch1_lock
            clients = self.ch1_clients
        else:
            buffer = self.ch2_buffer
            lock = self.ch2_lock
            clients = self.ch2_clients
        
        while self.running:
            if buffer and clients:
                try:
                    # Get latest value
                    value = buffer[-1]
                    data_str = f"{value:.12e}\n"  # Scientific notation for wide range
                    
                    with lock:
                        disconnected = []
                        for client in clients:
                            try:
                                client.sendall(data_str.encode('utf-8'))
                            except:
                                disconnected.append(client)
                        
                        for client in disconnected:
                            if client in clients:
                                clients.remove(client)
                    
                except Exception as e:
                    print(f"Broadcast error: {e}")
            
            time.sleep(0.1)  # Broadcast at 10Hz
    
    def _calculate_histogram(self, channel: int):
        """Calculate timing histogram."""
        while self.running:
            time.sleep(10)
            
            if channel == 1:
                times = list(self.ch1_elapsed_times)
                signal = self.ch1_histogram_updated
            else:
                times = list(self.ch2_elapsed_times)
                signal = self.ch2_histogram_updated
            
            if times:
                times_ms = [t * 1000 for t in times]
                hist, edges = np.histogram(times_ms, bins=20)
                signal.emit([hist.tolist(), edges.tolist()])
    
    def add_client(self, channel: int, client_socket):
        """Add client to channel."""
        if channel == 1:
            with self.ch1_lock:
                self.ch1_clients.append(client_socket)
                self.ch1_client_count_changed.emit(len(self.ch1_clients))
        else:
            with self.ch2_lock:
                self.ch2_clients.append(client_socket)
                self.ch2_client_count_changed.emit(len(self.ch2_clients))
    
    def remove_client(self, channel: int, client_socket):
        """Remove client from channel."""
        if channel == 1:
            with self.ch1_lock:
                if client_socket in self.ch1_clients:
                    self.ch1_clients.remove(client_socket)
                    self.ch1_client_count_changed.emit(len(self.ch1_clients))
        else:
            with self.ch2_lock:
                if client_socket in self.ch2_clients:
                    self.ch2_clients.remove(client_socket)
                    self.ch2_client_count_changed.emit(len(self.ch2_clients))
    
    def get_statistics(self, channel: int) -> Dict:
        """Get channel statistics."""
        if channel == 1:
            buffer = list(self.ch1_buffer)
            peak = self.ch1_peak
            count = self.ch1_data_count
        else:
            buffer = list(self.ch2_buffer)
            peak = self.ch2_peak
            count = self.ch2_data_count
        
        if buffer:
            return {
                'current': buffer[-1],
                'peak': peak,
                'average': np.mean(buffer),
                'std': np.std(buffer),
                'min': np.min(buffer),
                'max': np.max(buffer),
                'count': count
            }
        return {}