import socket
import threading
import time
import pyvisa
import pyqtgraph as pg
import numpy as np
from PyQt5.QtWidgets import QApplication, QVBoxLayout, QWidget
from PyQt5.QtCore import QObject, pyqtSignal, QTimer

class DataCollector(QObject):
    data_updated = pyqtSignal(float)
    histogram_updated = pyqtSignal(list)

    def __init__(self, multimeter, parent=None):
        super().__init__(parent)
        self.multimeter = multimeter
        self.running = False
        self.clients = []
        self.lock = threading.Lock()  # Lock for thread-safe client list operations
        self.elapsed_times = []

    def start(self):
        self.running = True
        data_thread = threading.Thread(target=self.collect_data)
        data_thread.start()
        
        histogram_thread = threading.Thread(target=self.calculate_histogram)
        histogram_thread.start()

    def collect_data(self):
        while self.running:
            try:
                start_time = time.time()
                # Get the current measurement from the multimeter - channel 1 only
                response = self.multimeter.query(":READ?")
                data_str = response.strip()
                
                # Just get the first value (channel 1)
                # If response contains multiple values separated by comma, get only the first one
                if ',' in data_str:
                    channel1_value = float(data_str.split(',')[0])
                else:
                    channel1_value = float(data_str)

                # Emit data to update client plot
                self.data_updated.emit(channel1_value)

                # Send only channel 1 data to clients
                with self.lock:
                    for client in self.clients.copy():  # Iterate over a copy of the client list
                        try:
                            client.sendall(f"{channel1_value}\n".encode('utf-8'))
                        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                            print("Client disconnected unexpectedly, removing from list.")
                            self.clients.remove(client)

                elapsed_time = time.time() - start_time
                self.elapsed_times.append(elapsed_time)

                time.sleep(0.05)  # Adjust the sleep time as needed

            except pyvisa.errors.VisaIOError as e:
                print(f"VISA IO Error: {e}")
            except ValueError as e:
                print(f"Value Error: {e} for response: {response}")
            except Exception as e:
                print(f"Unexpected error: {e}")

    def calculate_histogram(self):
        while self.running:
            time.sleep(10)  # Calculate histogram every 10 seconds
            with self.lock:
                if self.elapsed_times:
                    hist, bin_edges = np.histogram(self.elapsed_times, bins=20)
                    self.histogram_updated.emit([hist.tolist(), bin_edges.tolist()])
                    self.elapsed_times.clear()

    def stop(self):
        self.running = False

class DataPlotter(QWidget):
    def __init__(self):
        super().__init__()
        self.initUI()
        self.data_x = []
        self.max_data_points = 500

    def initUI(self):
        self.layout = QVBoxLayout(self)
        self.plotWidget = pg.GraphicsLayoutWidget()
        self.plotItem = self.plotWidget.addPlot(title="Real-time Channel 1 Current Measurement")
        self.curve = self.plotItem.plot(pen={'color': 'yellow', 'width': 3})
        self.histogramItem = self.plotWidget.addPlot(title="Histogram of Elapsed Times")
        self.histogram = pg.BarGraphItem(x=[], height=[], width=0.1, brush='r')
        self.histogramItem.addItem(self.histogram)
        self.layout.addWidget(self.plotWidget)

    def update_plot(self, data):
        self.data_x.append(data)
        self.curve.setData(self.data_x[-self.max_data_points:])

    def update_histogram(self, hist_data):
        hist, bin_edges = hist_data
        self.histogram.setOpts(x=bin_edges[:-1], height=hist, width=(bin_edges[1] - bin_edges[0]))

def main():
    app = QApplication([])

    # Connect to the Keithley multimeter
    rm = pyvisa.ResourceManager()
    multimeter = rm.open_resource('GPIB0::1::INSTR')
    
    # Configure multimeter to measure current on channel 1 only
    multimeter.write(":SENS:FUNC 'CURR'")
    # Add specific setup for channel 1 if needed
    # For example: multimeter.write(":SENS:CHAN 1") # If your multimeter supports this command

    # Set up data plotter
    data_plotter = DataPlotter()
    data_plotter.show()

    # Set up data collector
    data_collector = DataCollector(multimeter)
    data_collector.data_updated.connect(data_plotter.update_plot)
    data_collector.histogram_updated.connect(data_plotter.update_histogram)
    collector_thread = threading.Thread(target=data_collector.start)
    collector_thread.start()

    # Set up server
    host = "127.0.0.44"
    port = 55555
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((host, port))
    server.listen(5)
    print(f"Server listening on {host}:{port}")

    def accept_clients():
        while True:
            client_socket, addr = server.accept()
            print(f"Accepted connection from {addr}")
            print("Sending channel 1 data only to this client")
            with data_collector.lock:
                data_collector.clients.append(client_socket)
            client_handler = threading.Thread(target=handle_client, args=(client_socket, data_collector))
            client_handler.start()

    server_thread = threading.Thread(target=accept_clients)
    server_thread.start()

    def on_close():
        data_collector.stop()
        collector_thread.join()
        server.close()
        multimeter.close()
        app.quit()

    data_plotter.closeEvent = lambda event: on_close()

    app.exec_()

def handle_client(client_socket, data_collector):
    try:
        while True:
            time.sleep(1)  # Adjust this delay as needed
    except ConnectionResetError:
        print("Client disconnected")
    finally:
        with data_collector.lock:
            if client_socket in data_collector.clients:
                data_collector.clients.remove(client_socket)
        client_socket.close()

if __name__ == "__main__":
    main()