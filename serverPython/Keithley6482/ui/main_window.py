"""Main application window module."""

import threading
import time
from PyQt5.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                           QPushButton, QLabel, QStatusBar, QMessageBox,
                           QSystemTrayIcon, QApplication)
from PyQt5.QtCore import Qt

from core.data_collector import DataCollector
from core.server import DataServer
from hardware.multimeter import KeithleyMultimeter
from ui.data_plotter import DataPlotter
from ui.system_tray import SystemTrayIcon
import config


class MainWindow(QMainWindow):
    """Main application window."""
    
    def __init__(self):
        """Initialize the main window."""
        super().__init__()
        
        # Initialize components
        self.data_collector = None
        self.server = None
        self.multimeter = None
        self.is_closing = False
        
        # Setup UI
        self._init_ui()
        self._setup_system_tray()
        self._setup_server()
        self._setup_multimeter()
    
    def _init_ui(self):
        """Initialize the user interface."""
        self.setWindowTitle(config.APP_NAME)
        self.setGeometry(100, 100, 1000, 700)
        
        # Central widget
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # Main layout
        main_layout = QVBoxLayout(central_widget)
        
        # Control panel
        control_layout = QHBoxLayout()
        
        self.start_button = QPushButton('Start Collection')
        self.start_button.clicked.connect(self.start_collection)
        control_layout.addWidget(self.start_button)
        
        self.stop_button = QPushButton('Stop Collection')
        self.stop_button.clicked.connect(self.stop_collection)
        self.stop_button.setEnabled(False)
        control_layout.addWidget(self.stop_button)
        
        self.hide_button = QPushButton('Hide to Tray')
        self.hide_button.clicked.connect(self.hide_to_tray)
        control_layout.addWidget(self.hide_button)
        
        control_layout.addStretch()
        
        # Status labels
        self.client_label = QLabel('Connected Clients: 0')
        control_layout.addWidget(self.client_label)
        
        main_layout.addLayout(control_layout)
        
        # Data plotter
        self.data_plotter = DataPlotter(config.MAX_DATA_POINTS)
        main_layout.addWidget(self.data_plotter)
        
        # Status bar
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage('Ready - Click Start Collection to begin')
    
    def _setup_system_tray(self):
        """Setup system tray functionality."""
        if not QSystemTrayIcon.isSystemTrayAvailable():
            QMessageBox.critical(self, "System Tray",
                               "System tray is not available on this system.")
            return
        
        self.tray_icon = SystemTrayIcon(self)
        self.tray_icon.show()
        
        # Show startup message
        self.tray_icon.showMessage(
            'Data Collector',
            'Application started and available in system tray',
            QSystemTrayIcon.Information,
            2000
        )
    
    def _setup_multimeter(self):
        """Setup multimeter connection."""
        self.multimeter = KeithleyMultimeter(config.MULTIMETER_ADDRESS)
        success, message = self.multimeter.connect()
        
        if success:
            self.multimeter.configure(config.MEASUREMENT_FUNCTION)
            self.status_bar.showMessage(message)
            print(message)
        else:
            self.status_bar.showMessage(message)
            print(message)
            QMessageBox.warning(self, "Multimeter Connection", message)
    
    def _setup_server(self):
        """Setup the data server."""
        self.server = DataServer(config.SERVER_HOST, config.SERVER_PORT)
        success, message = self.server.setup()
        
        if success:
            self.server.start(self._handle_new_client)
            self.status_bar.showMessage(message)
            print(message)
        else:
            print(message)
            QMessageBox.critical(self, "Server Setup", message)
    
    def _handle_new_client(self, client_socket, address):
        """
        Handle a new client connection.
        
        Args:
            client_socket: Client socket object
            address: Client address
        """
        if self.data_collector:
            self.data_collector.add_client(client_socket)
            
            # Start client handler thread
            client_thread = threading.Thread(
                target=self._client_handler,
                args=(client_socket,),
                daemon=True
            )
            client_thread.start()
        else:
            # No active collection, close connection
            client_socket.close()
    
    def _client_handler(self, client_socket):
        """
        Handle individual client connection.
        
        Args:
            client_socket: Client socket object
        """
        try:
            while not self.is_closing and self.data_collector and self.data_collector.running:
                time.sleep(1)
        except (ConnectionResetError, ConnectionAbortedError):
            print("Client disconnected")
        finally:
            if self.data_collector:
                self.data_collector.remove_client(client_socket)
            try:
                client_socket.close()
            except:
                pass
    
    def start_collection(self):
        """Start data collection."""
        if not self.multimeter:
            QMessageBox.warning(self, "Error", "Multimeter not connected!")
            return
        
        # Create and start data collector
        self.data_collector = DataCollector(self.multimeter)
        self.data_collector.measurement_delay = config.MEASUREMENT_DELAY
        self.data_collector.histogram_interval = config.HISTOGRAM_UPDATE_INTERVAL
        
        # Connect signals
        self.data_collector.data_updated.connect(self.data_plotter.update_plot)
        self.data_collector.histogram_updated.connect(self.data_plotter.update_histogram)
        self.data_collector.client_count_changed.connect(self._update_client_count)
        
        self.data_collector.start()
        
        # Update UI
        self.start_button.setEnabled(False)
        self.stop_button.setEnabled(True)
        self.status_bar.showMessage('Data collection started')
        
        # Update tray icon
        if hasattr(self, 'tray_icon'):
            self.tray_icon.update_status(True)
    
    def stop_collection(self):
        """Stop data collection."""
        if self.data_collector:
            self.data_collector.stop()
            self.data_collector = None
        
        # Update UI
        self.start_button.setEnabled(True)
        self.stop_button.setEnabled(False)
        self.status_bar.showMessage('Data collection stopped')
        
        # Update tray icon
        if hasattr(self, 'tray_icon'):
            self.tray_icon.update_status(False)
    
    def _update_client_count(self, count: int):
        """Update the client count display."""
        self.client_label.setText(f'Connected Clients: {count}')
    
    def hide_to_tray(self):
        """Hide window to system tray."""
        if hasattr(self, 'tray_icon'):
            self.hide()
            self.tray_icon.showMessage(
                'Data Collector',
                'Application minimized to system tray',
                QSystemTrayIcon.Information,
                2000
            )
        else:
            QMessageBox.information(self, "System Tray",
                                  "System tray not available. Window will be minimized.")
            self.showMinimized()
    
    def get_status(self) -> str:
        """Get current application status."""
        if self.data_collector:
            client_count = self.data_collector.get_client_count()
            data_count = self.data_collector.data_count
            return f"Clients Connected: {client_count}\nData Points Collected: {data_count}"
        else:
            return "Data collection not active"
    
    def closeEvent(self, event):
        """Handle window close event."""
        if hasattr(self, 'tray_icon') and self.tray_icon.isVisible():
            event.ignore()
            self.hide_to_tray()
        else:
            reply = QMessageBox.question(self, 'Exit Application',
                                       'Are you sure you want to exit?',
                                       QMessageBox.Yes | QMessageBox.No,
                                       QMessageBox.No)
            
            if reply == QMessageBox.Yes:
                self.close_application()
                event.accept()
            else:
                event.ignore()
    
    def close_application(self):
        """Completely close the application."""
        self.is_closing = True
        
        # Stop data collection
        if self.data_collector:
            self.data_collector.stop()
        
        # Stop server
        if self.server:
            self.server.stop()
        
        # Close multimeter
        if self.multimeter:
            self.multimeter.close()
        
        # Hide tray icon
        if hasattr(self, 'tray_icon'):
            self.tray_icon.hide()
        
        QApplication.quit()