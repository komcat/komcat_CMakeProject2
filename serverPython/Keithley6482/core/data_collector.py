"""Data collection module for handling multimeter measurements."""

import threading
import time
import numpy as np
from typing import List, Optional
from PyQt5.QtCore import QObject, pyqtSignal


class DataCollector(QObject):
    """Handles data collection from the multimeter and client distribution."""
    
    # Signals
    data_updated = pyqtSignal(float)
    histogram_updated = pyqtSignal(list)
    client_count_changed = pyqtSignal(int)
    
    def __init__(self, multimeter, parent=None):
        """
        Initialize the data collector.
        
        Args:
            multimeter: Multimeter interface instance
            parent: Parent QObject
        """
        super().__init__(parent)
        self.multimeter = multimeter
        self.running = False
        self.clients = []
        self.lock = threading.Lock()
        self.elapsed_times = []
        self.data_count = 0
        self.measurement_delay = 0.05
        self.histogram_interval = 10
        
    def start(self):
        """Start data collection threads."""
        self.running = True
        
        # Start data collection thread
        data_thread = threading.Thread(target=self._collect_data, daemon=True)
        data_thread.start()
        
        # Start histogram calculation thread
        histogram_thread = threading.Thread(target=self._calculate_histogram, daemon=True)
        histogram_thread.start()
    
    def stop(self):
        """Stop data collection."""
        self.running = False
    
    def _collect_data(self):
        """Main data collection loop."""
        while self.running:
            start_time = time.time()
            
            # Get measurement from multimeter
            value = self.multimeter.read_measurement()
            
            if value is not None:
                # Emit signal for UI update
                self.data_updated.emit(value)
                self.data_count += 1
                
                # Send to connected clients
                self._broadcast_to_clients(value)
                
                # Track timing
                elapsed_time = time.time() - start_time
                self.elapsed_times.append(elapsed_time)
            
            time.sleep(self.measurement_delay)
    
    def _broadcast_to_clients(self, value: float):
        """
        Send data to all connected clients.
        
        Args:
            value: Measurement value to send
        """
        with self.lock:
            disconnected = []
            
            for client in self.clients:
                try:
                    client.sendall(f"{value}\n".encode('utf-8'))
                except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                    print("Client disconnected unexpectedly")
                    disconnected.append(client)
            
            # Remove disconnected clients
            for client in disconnected:
                if client in self.clients:
                    self.clients.remove(client)
            
            if disconnected:
                self.client_count_changed.emit(len(self.clients))
    
    def _calculate_histogram(self):
        """Calculate and emit histogram data periodically."""
        while self.running:
            time.sleep(self.histogram_interval)
            
            with self.lock:
                if self.elapsed_times:
                    hist, bin_edges = np.histogram(self.elapsed_times, bins=20)
                    self.histogram_updated.emit([hist.tolist(), bin_edges.tolist()])
                    self.elapsed_times.clear()
    
    def add_client(self, client_socket):
        """Add a new client socket."""
        with self.lock:
            self.clients.append(client_socket)
            self.client_count_changed.emit(len(self.clients))
    
    def remove_client(self, client_socket):
        """Remove a client socket."""
        with self.lock:
            if client_socket in self.clients:
                self.clients.remove(client_socket)
                self.client_count_changed.emit(len(self.clients))
    
    def get_client_count(self) -> int:
        """Get the current number of connected clients."""
        with self.lock:
            return len(self.clients)