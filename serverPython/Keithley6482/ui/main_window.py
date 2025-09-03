"""Main application window module with dual channel support."""

import threading
import time
from PyQt5.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                           QPushButton, QLabel, QStatusBar, QMessageBox,
                           QSystemTrayIcon, QApplication, QGroupBox, QGridLayout)
from PyQt5.QtCore import Qt

from core.data_collector import DualChannelDataCollector
from core.server import DataServer
from hardware.multimeter import KeithleyMultimeter
from ui.dual_channel_plotter import DualChannelPlotter
from ui.system_tray import SystemTrayIcon
import config


class MainWindow(QMainWindow):
    """Main application window with dual channel support."""
    
    def __init__(self):
        """Initialize the main window."""
        super().__init__()
        
        # Initialize components
        self.data_collector = None
        self.ch1_server = None
        self.ch2_server = None
        self.multimeter = None
        self.is_closing = False
        
        # Setup UI
        self._init_ui()
        self._setup_system_tray()
        self._setup_servers()
        self._setup_multimeter()
    
    def _init_ui(self):
        """Initialize the user interface."""
        self.setWindowTitle(config.APP_NAME)
        self.setGeometry(100, 100, 1200, 800)
        
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
        
        main_layout.addLayout(control_layout)
        
        # Server status panel
        status_group = QGroupBox("Server Status")
        status_layout = QGridLayout()
        
        # Channel 1 status
        status_layout.addWidget(QLabel("Channel 1:"), 0, 0)
        self.ch1_server_label = QLabel(f"Server: {config.SERVER_CH1_HOST}:{config.SERVER_CH1_PORT}")
        status_layout.addWidget(self.ch1_server_label, 0, 1)
        self.ch1_client_label = QLabel('Clients: 0')
        status_layout.addWidget(self.ch1_client_label, 0, 2)
        self.ch1_data_label = QLabel('Data Points: 0')
        status_layout.addWidget(self.ch1_data_label, 0, 3)
        
        # Channel 2 status
        status_layout.addWidget(QLabel("Channel 2:"), 1, 0)
        self.ch2_server_label = QLabel(f"Server: {config.SERVER_CH2_HOST}:{config.SERVER_CH2_PORT}")
        status_layout.addWidget(self.ch2_server_label, 1, 1)
        self.ch2_client_label = QLabel('Clients: 0')
        status_layout.addWidget(self.ch2_client_label, 1, 2)
        self.ch2_data_label = QLabel('Data Points: 0')
        status_layout.addWidget(self.ch2_data_label, 1, 3)
        
        status_group.setLayout(status_layout)
        main_layout.addWidget(status_group)
        
        # Dual channel plotter
        self.data_plotter = DualChannelPlotter(config.MAX_DATA_POINTS)
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
            'Dual Channel Data Collector',
            'Application started and available in system tray',
            QSystemTrayIcon.Information,
            2000
        )
    
    def _setup_multimeter(self):
        """Setup multimeter connection."""
        self.multimeter = KeithleyMultimeter(config.MULTIMETER_ADDRESS)
        success, message = self.multimeter.connect()
        
        if success:
            self.multimeter.configure(
                config.MEASUREMENT_FUNCTION,
                config.ENABLED_CHANNELS
            )
            self.status_bar.showMessage(message)
            print(message)
        else:
            self.status_bar.showMessage(message)
            print(message)
            QMessageBox.warning(self, "Multimeter Connection", message)
    
    def _setup_servers(self):
        """Setup the data servers for both channels."""
        # Setup channel 1 server
        self.ch1_server = DataServer(config.SERVER_CH1_HOST, config.SERVER_CH1_PORT)
        success1, message1 = self.ch1_server.setup()
        
        if success1:
            self.ch1_server.start(lambda sock, addr: self._handle_new_client(1, sock, addr))
            print(f"Channel 1: {message1}")
        else:
            print(f"Channel 1 server failed: {message1}")
            QMessageBox.warning(self, "Channel 1 Server", message1)
        
        # Setup channel 2 server
        self.ch2_server = DataServer(config.SERVER_CH2_HOST, config.SERVER_CH2_PORT)
        success2, message2 = self.ch2_server.setup()
        
        if success2:
            self.ch2_server.start(lambda sock, addr: self._handle_new_client(2, sock, addr))
            print(f"Channel 2: {message2}")
        else:
            print(f"Channel 2 server failed: {message2}")
            QMessageBox.warning(self, "Channel 2 Server", message2)
        
        if success1 and success2:
            self.status_bar.showMessage("Both channel servers ready")
        elif success1:
            self.status_bar.showMessage("Only Channel 1 server ready")
        elif success2:
            self.status_bar.showMessage("Only Channel 2 server ready")
        else:
            self.status_bar.showMessage("Server setup failed")
    
    def _handle_new_client(self, channel: int, client_socket, address):
        """
        Handle a new client connection for a specific channel.
        
        Args:
            channel: Channel number (1 or 2)
            client_socket: Client socket object
            address: Client address
        """
        print(f"Channel {channel}: New client from {address}")
        
        if self.data_collector:
            self.data_collector.add_client(channel, client_socket)
            
            # Start client handler thread
            client_thread = threading.Thread(
                target=self._client_handler,
                args=(channel, client_socket),
                daemon=True
            )
            client_thread.start()
        else:
            # No active collection, close connection
            client_socket.close()
    
    def _client_handler(self, channel: int, client_socket):
        """
        Handle individual client connection.
        
        Args:
            channel: Channel number (1 or 2)
            client_socket: Client socket object
        """
        try:
            while not self.is_closing and self.data_collector and self.data_collector.running:
                time.sleep(1)
        except (ConnectionResetError, ConnectionAbortedError):
            print(f"Channel {channel} client disconnected")
        finally:
            if self.data_collector:
                self.data_collector.remove_client(channel, client_socket)
            try:
                client_socket.close()
            except:
                pass
    
    def start_collection(self):
        """Start data collection."""
        if not self.multimeter:
            QMessageBox.warning(self, "Error", "Multimeter not connected!")
            return
        
        # Create and start dual channel data collector
        self.data_collector = DualChannelDataCollector(self.multimeter)
        self.data_collector.measurement_delay = config.MEASUREMENT_DELAY
        self.data_collector.histogram_interval = config.HISTOGRAM_UPDATE_INTERVAL
        
        # Connect channel 1 signals
        self.data_collector.ch1_data_updated.connect(self.data_plotter.update_ch1_plot)
        self.data_collector.ch1_histogram_updated.connect(self.data_plotter.update_ch1_histogram)
        self.data_collector.ch1_client_count_changed.connect(
            lambda count: self.ch1_client_label.setText(f'Clients: {count}')
        )
        
        # Connect channel 2 signals
        self.data_collector.ch2_data_updated.connect(self.data_plotter.update_ch2_plot)
        self.data_collector.ch2_histogram_updated.connect(self.data_plotter.update_ch2_histogram)
        self.data_collector.ch2_client_count_changed.connect(
            lambda count: self.ch2_client_label.setText(f'Clients: {count}')
        )
        
        # Start collection
        self.data_collector.start()
        
        # Start update timer for data counts
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self._update_data_counts)
        self.update_timer.start(1000)  # Update every second
        
        # Update UI
        self.start_button.setEnabled(False)
        self.stop_button.setEnabled(True)
        self.status_bar.showMessage('Data collection started on both channels')
        
        # Update tray icon
        if hasattr(self, 'tray_icon'):
            self.tray_icon.update_status(True)
    
    def stop_collection(self):
        """Stop data collection."""
        if hasattr(self, 'update_timer'):
            self.update_timer.stop()
            
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
    
    def _update_data_counts(self):
        """Update data count displays."""
        if self.data_collector:
            self.ch1_data_label.setText(f'Data Points: {self.data_collector.ch1_data_count}')
            self.ch2_data_label.setText(f'Data Points: {self.data_collector.ch2_data_count}')
    
    def hide_to_tray(self):
        """Hide window to system tray."""
        if hasattr(self, 'tray_icon'):
            self.hide()
            self.tray_icon.showMessage(
                'Dual Channel Data Collector',
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
            ch1_clients = self.data_collector.get_client_count(1)
            ch2_clients = self.data_collector.get_client_count(2)
            ch1_data = self.data_collector.ch1_data_count
            ch2_data = self.data_collector.ch2_data_count
            
            return (f"Channel 1: {ch1_clients} clients, {ch1_data} points\n"
                   f"Channel 2: {ch2_clients} clients, {ch2_data} points")
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
        
        # Stop servers
        if self.ch1_server:
            self.ch1_server.stop()
        if self.ch2_server:
            self.ch2_server.stop()
        
        # Close multimeter
        if self.multimeter:
            self.multimeter.close()
        
        # Hide tray icon
        if hasattr(self, 'tray_icon'):
            self.tray_icon.hide()
        
        QApplication.quit()


# Add missing import for QTimer
from PyQt5.QtCore import QTimer