from PyQt6.QtWidgets import QMainWindow, QVBoxLayout, QWidget, QPushButton, QHBoxLayout, QDialog, QTextEdit, QPushButton
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
from data_processing.utils import IRAN_TZ
import numpy as np

class RecordDetailsDialog(QDialog):
    """Dialog to display details of a selected record."""
    def __init__(self, record, interval, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Record Details")
        self.setGeometry(300, 300, 400, 200)
        layout = QVBoxLayout()
        self.setLayout(layout)

        details_text = QTextEdit()
        details_text.setReadOnly(True)
        timestamp, count, pulse_duration, send_duration = record
        delayed_note = "<b>Note:</b> Count > 1 indicates delayed data send from previous attempts.<br>" if count > 1 else ""
        details = (
            f"<b>Timestamp:</b> {timestamp.astimezone(IRAN_TZ).strftime('%Y-%m-%d %H:%M:%S')}<br>"
            f"<b>Production Count:</b> {count}<br>"
            f"{delayed_note}"
            f"<b>Pulse Duration:</b> {pulse_duration:.2f} ms<br>"
            f"<b>Send Duration:</b> {send_duration:.2f} ms<br>"
            f"<b>Interval to Previous:</b> {interval:.2f} min" if interval is not None else "<b>Interval:</b> N/A (First record)"
        )
        details_text.setHtml(details)
        layout.addWidget(details_text)

        close_button = QPushButton("Close")
        close_button.clicked.connect(self.accept)
        layout.addWidget(close_button)

class PlotWindow(QMainWindow):
    """Window to display production data plots."""
    def __init__(self, record_times, intervals, pulse_durations, send_durations, filtered_records, anomalies, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Production Data Plots")
        self.setGeometry(200, 200, 800, 600)
        self.record_times = record_times
        self.intervals = intervals
        self.pulse_durations = pulse_durations
        self.send_durations = send_durations
        self.filtered_records = filtered_records
        self.anomalies = anomalies

        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.layout = QVBoxLayout(self.central_widget)

        # Plot selection buttons
        button_layout = QHBoxLayout()
        self.count_button = QPushButton("Production Count")
        self.count_button.clicked.connect(self.plot_count)
        self.interval_button = QPushButton("Intervals")
        self.interval_button.clicked.connect(self.plot_intervals)
        self.pulse_button = QPushButton("Pulse Durations")
        self.pulse_button.clicked.connect(self.plot_pulse_durations)
        self.send_button = QPushButton("Send Durations")
        self.send_button.clicked.connect(self.plot_send_durations)
        button_layout.addWidget(self.count_button)
        button_layout.addWidget(self.interval_button)
        button_layout.addWidget(self.pulse_button)
        button_layout.addWidget(self.send_button)
        self.layout.addLayout(button_layout)

        # Matplotlib canvas
        self.figure = Figure()
        self.canvas = FigureCanvas(self.figure)
        self.canvas.mpl_connect('button_press_event', self.on_plot_click)
        self.layout.addWidget(self.canvas)

        # Plot the first type by default
        self.plot_count()
        self.current_plot = 'count'

    def on_plot_click(self, event):
        """Handles click events on the plot to show record details."""
        if not event.xdata or not event.ydata:
            return

        times = [t.astimezone(IRAN_TZ).timestamp() for t in self.record_times]
        if self.current_plot == 'count':
            y_values = [c for _, c, _, _ in self.filtered_records]
        elif self.current_plot == 'pulse':
            y_values = self.pulse_durations
        elif self.current_plot == 'send':
            y_values = self.send_durations
        else:
            return  # Intervals plot doesn't support point selection

        # Find the closest point
        x_clicked = event.xdata
        y_clicked = event.ydata
        distances = [(abs(t - x_clicked) ** 2 + abs(y - y_clicked) ** 2, i)
                     for i, (t, y) in enumerate(zip(times, y_values))]
        if not distances:
            return
        _, closest_idx = min(distances)

        # Get record details
        record = self.filtered_records[closest_idx]
        interval = self.intervals[closest_idx - 1] if closest_idx > 0 else None
        dialog = RecordDetailsDialog(record, interval, self)
        dialog.exec()

    def plot_count(self):
        """Plots production count over time."""
        self.current_plot = 'count'
        self.figure.clear()
        ax = self.figure.add_subplot(111)
        times = [t.astimezone(IRAN_TZ) for t, _, _, _ in self.filtered_records]
        counts = [c for _, c, _, _ in self.filtered_records]
        ax.plot(times, counts, label="Production Count", color="blue")
        anomaly_times = [self.filtered_records[i][0].astimezone(IRAN_TZ) for i in self.anomalies]
        anomaly_counts = [self.filtered_records[i][1] for i in self.anomalies]
        ax.scatter(anomaly_times, anomaly_counts, color="red", label="Anomalies (Pulse or Count > 1)")
        ax.set_xlabel("Time")
        ax.set_ylabel("Production Count")
        ax.set_title("Production Count Over Time")
        ax.legend()
        self.figure.autofmt_xdate()
        self.canvas.draw()

    def plot_intervals(self):
        """Plots intervals between records."""
        self.current_plot = 'intervals'
        self.figure.clear()
        ax = self.figure.add_subplot(111)
        times = [self.record_times[i].astimezone(IRAN_TZ) for i in range(len(self.intervals))]
        ax.plot(times, self.intervals, label="Intervals", color="green")
        ax.set_xlabel("Time")
        ax.set_ylabel("Interval (minutes)")
        ax.set_title("Intervals Between Production Records")
        ax.legend()
        self.figure.autofmt_xdate()
        self.canvas.draw()

    def plot_pulse_durations(self):
        """Plots pulse durations with anomalies."""
        self.current_plot = 'pulse'
        self.figure.clear()
        ax = self.figure.add_subplot(111)
        times = [t.astimezone(IRAN_TZ) for t in self.record_times]
        ax.plot(times, self.pulse_durations, label="Pulse Durations", color="purple")
        anomaly_times = [self.record_times[i].astimezone(IRAN_TZ) for i in self.anomalies]
        anomaly_pulses = [self.pulse_durations[i] for i in self.anomalies]
        ax.scatter(anomaly_times, anomaly_pulses, color="red", label="Anomalies (Pulse or Count > 1)")
        ax.set_xlabel("Time")
        ax.set_ylabel("Pulse Duration (ms)")
        ax.set_title("Pulse Durations Over Time")
        ax.legend()
        self.figure.autofmt_xdate()
        self.canvas.draw()

    def plot_send_durations(self):
        """Plots send durations."""
        self.current_plot = 'send'
        self.figure.clear()
        ax = self.figure.add_subplot(111)
        times = [t.astimezone(IRAN_TZ) for t in self.record_times]
        ax.plot(times, self.send_durations, label="Send Durations", color="orange")
        ax.set_xlabel("Time")
        ax.set_ylabel("Send Duration (ms)")
        ax.set_title("Send Durations Over Time")
        ax.legend()
        self.figure.autofmt_xdate()
        self.canvas.draw()