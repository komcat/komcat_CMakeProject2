"""Configuration settings for the Keithley Data Collector application."""

# Server Configuration
SERVER_HOST = "127.0.0.44"
SERVER_PORT = 55555
SERVER_LISTEN_BACKLOG = 5

# Multimeter Configuration
MULTIMETER_ADDRESS = 'GPIB0::1::INSTR'
MEASUREMENT_FUNCTION = 'CURR'  # Current measurement
MEASUREMENT_DELAY = 0.05  # Delay between measurements in seconds

# UI Configuration
MAX_DATA_POINTS = 500  # Maximum data points to display in plot
HISTOGRAM_UPDATE_INTERVAL = 10  # Seconds between histogram updates
PLOT_LINE_COLOR = 'yellow'
PLOT_LINE_WIDTH = 3
HISTOGRAM_COLOR = 'r'

# Application Settings
APP_NAME = "Data Collector - Keithley Multimeter"
TRAY_TOOLTIP_RUNNING = "Data Collector - Running"
TRAY_TOOLTIP_STOPPED = "Data Collector - Stopped"
TRAY_TOOLTIP_COLLECTING = "Data Collector - Collecting Data"