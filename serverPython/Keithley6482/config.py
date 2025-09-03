"""Configuration settings for the Keithley Data Collector application."""

# Server Configuration for Channel 1
SERVER_CH1_HOST = "127.0.0.44"
SERVER_CH1_PORT = 55555

# Server Configuration for Channel 2
SERVER_CH2_HOST = "127.0.0.45"  # Different IP for channel 2
SERVER_CH2_PORT = 55556  # Different port for channel 2

SERVER_LISTEN_BACKLOG = 5

# Multimeter Configuration
MULTIMETER_ADDRESS = 'GPIB0::1::INSTR'
MEASUREMENT_FUNCTION = 'CURR'  # Current measurement
MEASUREMENT_DELAY = 0.05  # Delay between measurements in seconds
ENABLED_CHANNELS = [1, 2]  # List of enabled channels

# UI Configuration
MAX_DATA_POINTS = 500  # Maximum data points to display in plot
HISTOGRAM_UPDATE_INTERVAL = 10  # Seconds between histogram updates
PLOT_LINE_COLOR_CH1 = 'yellow'
PLOT_LINE_COLOR_CH2 = 'cyan'
PLOT_LINE_WIDTH = 3
HISTOGRAM_COLOR = 'r'

# Application Settings
APP_NAME = "Dual Channel Data Collector - Keithley 6482"
TRAY_TOOLTIP_RUNNING = "Data Collector - Running"
TRAY_TOOLTIP_STOPPED = "Data Collector - Stopped"
TRAY_TOOLTIP_COLLECTING = "Data Collector - Collecting Data"