from PyQt6.QtWidgets import QDialog, QVBoxLayout, QPushButton
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import seaborn as sns
import pandas as pd
from influxdb_fetch.queries import query_rssi_data
from data_processing.utils import IRAN_TZ

class RSSIHeatmapDialog(QDialog):
    """Dialog to display a 3-axis heatmap of WiFi RSSI values."""
    def __init__(self, device_ids, start_date, end_date, parent=None):
        super().__init__(parent)
        self.setWindowTitle("WiFi RSSI Heatmap")
        self.setGeometry(200, 200, 800, 600)
        self.device_ids = device_ids
        self.start_date = start_date
        self.end_date = end_date

        layout = QVBoxLayout()
        self.setLayout(layout)

        # Matplotlib canvas
        self.figure = Figure()
        self.canvas = FigureCanvas(self.figure)
        layout.addWidget(self.canvas)

        close_button = QPushButton("Close")
        close_button.clicked.connect(self.accept)
        layout.addWidget(close_button)

        self.plot_heatmap()

    def plot_heatmap(self):
        """Plots a 3-axis heatmap of RSSI values."""
        self.figure.clear()
        ax = self.figure.add_subplot(111, projection='3d')

        # Query RSSI data
        rssi_data = query_rssi_data(self.device_ids, self.start_date, self.end_date)
        if not rssi_data:
            ax.text(0.5, 0.5, 0.5, "No RSSI data available", ha='center', va='center')
            self.canvas.draw()
            return

        # Prepare data for 3D heatmap
        df = pd.DataFrame(rssi_data, columns=['Device', 'Date', 'Hour', 'RSSI'])
        df['Date'] = pd.to_datetime(df['Date']).dt.date
        df['Hour'] = df['Hour'].astype(int)
        pivot = df.pivot_table(values='RSSI', index=['Device', 'Date'], columns='Hour', aggfunc='mean', fill_value=0)

        # Extract data for plotting
        devices = pivot.index.get_level_values(0).unique()
        dates = pivot.index.get_level_values(1).unique()
        hours = pivot.columns
        X, Y = np.meshgrid(hours, range(len(devices) * len(dates)))
        Z = np.zeros_like(X)

        device_date_labels = [f"{d} {dt}" for d in devices for dt in dates]
        for i, (device, date) in enumerate(pivot.index):
            for hour in hours:
                Z[i, hour] = pivot.loc[(device, date), hour] if (device, date, hour) in pivot else 0

        # Plot 3D surface
        surf = ax.plot_surface(X, Y, Z, cmap='viridis')
        ax.set_xlabel('Hour of Day')
        ax.set_ylabel('Device & Date')
        ax.set_zlabel('RSSI (dBm)')
        ax.set_yticks(range(len(device_date_labels)))
        ax.set_yticklabels(device_date_labels, rotation=45, ha='right')
        self.figure.colorbar(surf, ax=ax, label='RSSI (dBm)')
        self.canvas.draw()