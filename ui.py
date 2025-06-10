import sys
from datetime import datetime, timedelta, timezone
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                            QLabel, QLineEdit, QPushButton, QGroupBox, QDateEdit, QMessageBox,
                            QComboBox, QTableWidget, QTableWidgetItem, QDialog, QGridLayout)
from PyQt6.QtCore import Qt, QDate, pyqtSignal
from PyQt6.QtGui import QFont
from influxdb_client import InfluxDBClient
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import matplotlib.dates as mdates
import numpy as np
import pandas as pd
import os

# --- Configuration ---
INFLUXDB_URL = "http://192.168.1.37:8086"
INFLUXDB_TOKEN = "_2B1s0cJGXznc9rXE41mxOusyn159HAAsMyGlLxiFkZd0V8dJKztKYVEPNeKGQSgc0gulh-vhnhn6ONcCr3lBA=="
INFLUXDB_ORG = "tooswasher"
INFLUXDB_BUCKET = "manufacture"
FIELD_NAME = "count"
MEASUREMENT = "production"
DEVICE_INFO_MEASUREMENT = "device_info"

UTC_TZ = timezone.utc
IRAN_TZ = timezone(timedelta(hours=3, minutes=30))  # UTC+3:30

def get_signal_quality(rssi):
    """Convert RSSI value to human-readable signal quality."""
    if rssi >= -50:
        return "Excellent"
    if rssi >= -60:
        return "Good"
    if rssi >= -70:
        return "Fair"
    return "Weak"

def escape_flux_string(s: str) -> str:
    """
    Escapes a string for safe inclusion within a Flux query string literal.
    Handles backslashes and double quotes.
    """
    if s is None:
        return ""
    # Ensure it's a string, then replace special characters
    s_str = str(s)
    return s_str.replace('\\', '\\\\').replace('"', '\\"')

class RecordsDialog(QDialog):
    """
    A dialog to display nearby production records for a selected point on the plot.
    Shows 10 records before and 10 records after the selected point, highlighting the selected one.
    """
    def __init__(self, records, selected_idx, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Nearby Records")
        self.setGeometry(200, 200, 600, 400)
        layout = QVBoxLayout(self)

        # Create table to display records
        self.table = QTableWidget()
        self.table.setRowCount(21)  # Up to 10 before + selected + 10 after
        self.table.setColumnCount(2)
        self.table.setHorizontalHeaderLabels(["Time (Iran TZ, UTC+3:30)", "Count"])
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.setStyleSheet("QTableWidget { color: #000000; }") # Ensure text is readable
        layout.addWidget(self.table)

        # Populate table with records around the selected index
        start_idx = max(0, selected_idx - 10)
        end_idx = min(len(records), selected_idx + 11)
        for i, idx in enumerate(range(start_idx, end_idx)):
            if idx < len(records): # Ensure index is within bounds (safety check)
                time, count = records[idx]
                time_item = QTableWidgetItem(time.astimezone(IRAN_TZ).strftime('%Y-%m-%d %H:%M:%S'))
                count_item = QTableWidgetItem(str(count))
                if idx == selected_idx:
                    # Highlight the selected row
                    time_item.setBackground(Qt.GlobalColor.yellow)
                    count_item.setBackground(Qt.GlobalColor.yellow)
                self.table.setItem(i, 0, time_item)
                self.table.setItem(i, 1, count_item)
        self.table.resizeColumnsToContents()

class StartupRecordsDialog(QDialog):
    """
    A dialog to display device startup records (time, IP address, and signal quality).
    """
    def __init__(self, startup_records, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Device Startup Records")
        self.setGeometry(200, 200, 800, 400)
        layout = QVBoxLayout(self)

        # Create table to display startup records
        self.table = QTableWidget()
        self.table.setRowCount(len(startup_records))
        self.table.setColumnCount(3)
        self.table.setHorizontalHeaderLabels(["Time (Iran TZ, UTC+3:30)", "IP Address", "Signal Quality"])
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.setStyleSheet("QTableWidget { color: #000000; }") # Ensure text is readable
        layout.addWidget(self.table)

        # Populate table with startup records
        for i, (time, ip, rssi) in enumerate(startup_records):
            time_item = QTableWidgetItem(time.astimezone(IRAN_TZ).strftime('%Y-%m-%d %H:%M:%S'))
            ip_item = QTableWidgetItem(ip)
            signal_item = QTableWidgetItem(f"{get_signal_quality(rssi)} ({rssi} dBm)")
            self.table.setItem(i, 0, time_item)
            self.table.setItem(i, 1, ip_item)
            self.table.setItem(i, 2, signal_item)
        self.table.resizeColumnsToContents()

class PlotSettingsDialog(QDialog):
    """
    A dialog for adjusting plot settings, specifically the threshold percentage.
    Emits a signal when the threshold is changed and applied.
    """
    threshold_changed = pyqtSignal(float) # Signal to emit the new threshold

    def __init__(self, current_threshold, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Plot Settings")
        self.setFixedSize(300, 100) # Fixed size for simplicity
        layout = QVBoxLayout(self)

        # Layout for threshold input
        threshold_layout = QHBoxLayout()
        self.threshold_label = QLabel("Threshold (%):")
        self.threshold_entry = QLineEdit(str(current_threshold))
        self.threshold_entry.setToolTip("Percentage above/below average for highlighting outliers")
        self.threshold_entry.setMaximumWidth(60) # Keep input field narrow
        threshold_layout.addWidget(self.threshold_label)
        threshold_layout.addWidget(self.threshold_entry)
        threshold_layout.addStretch() # Push widgets to the left
        layout.addLayout(threshold_layout)

        # Apply button
        apply_button = QPushButton("Apply")
        apply_button.clicked.connect(self.apply_settings)
        layout.addWidget(apply_button)

    def apply_settings(self):
        """
        Validates the input threshold, emits the threshold_changed signal, and closes the dialog.
        Shows a warning if input is invalid.
        """
        try:
            threshold = float(self.threshold_entry.text().strip())
            if threshold < 0:
                raise ValueError("Threshold must be non-negative")
            self.threshold_changed.emit(threshold) # Emit the new threshold value
            self.accept() # Close the dialog
        except ValueError:
            QMessageBox.warning(self, "Invalid Input", "Please enter a valid non-negative percentage for the threshold.")


class PlotCanvas(FigureCanvas):
    """
    A Matplotlib canvas embedded in a PyQt application for displaying time interval plots.
    Supports hover annotations and click-to-show nearby records.
    Emits a signal when plot statistics are updated.
    """
    plot_stats_updated = pyqtSignal(dict) # New signal to emit plot statistics

    def __init__(self, parent=None, width=8, height=4, dpi=100, records=None, main_window=None):
        self.fig = Figure(figsize=(width, height), dpi=dpi, facecolor='#f5f6f5') # Figure for the plot
        super().__init__(self.fig)
        self.setParent(parent)
        self.ax = self.fig.add_subplot(111) # Add a subplot to the figure
        self.annotation = None # Annotation for hover effect
        self.record_times = [] # Times of records
        self.intervals = [] # Calculated intervals
        self.records = records or [] # Original records (time, count)
        self.main_window = main_window # Reference to the main window for RecordsDialog
        self.threshold_percent = 10 # Default threshold percentage

    def plot(self, record_times, intervals, records):
        """
        Generates and updates the plot with time intervals.
        Highlights points based on the current threshold.
        Calculates and emits plot statistics.
        """
        self.ax.clear() # Clear previous plot
        self.record_times = record_times
        self.intervals = intervals
        self.records = records

        plot_statistics = {
            "avg_interval_above_threshold": 0,
            "avg_interval_below_threshold": 0,
            "avg_interval_within_threshold": 0,
            "percent_above_threshold": 0,
            "percent_below_threshold": 0,
            "percent_within_threshold": 0,
            "total_intervals": 0
        }

        if not intervals:
            self.draw()
            self.plot_stats_updated.emit(plot_statistics) # Emit empty stats
            return

        # Calculate average interval and dynamic thresholds based on the percentage
        avg_interval = np.mean(intervals)
        threshold_factor = self.threshold_percent / 100
        upper_threshold = avg_interval * (1 + threshold_factor)
        lower_threshold = avg_interval * (1 - threshold_factor)

        intervals_above = []
        intervals_below = []
        intervals_within = []

        # Plot each point with conditional coloring (red for above, green for below, blue for within)
        for i in range(len(intervals)):
            if intervals[i] > upper_threshold:
                color = 'red'  # Above threshold
                intervals_above.append(intervals[i])
            elif intervals[i] < lower_threshold:
                color = 'green'  # Below threshold
                intervals_below.append(intervals[i])
            else:
                color = 'blue'  # Within threshold
                intervals_within.append(intervals[i])
            self.ax.plot(record_times[i], intervals[i], marker='o', linestyle='', color=color,
                         markersize=8, markeredgecolor='#1a3c34', markeredgewidth=1)
        # Plot the connecting lines
        self.ax.plot(record_times, intervals, linestyle='-', color='#2a9d8f', linewidth=2.5, alpha=0.5)

        # Plot horizontal lines for average and thresholds
        self.ax.axhline(y=upper_threshold, color='red', linestyle='--', alpha=0.5,
                       label=f'Upper Threshold ({self.threshold_percent}% above avg: {upper_threshold:.2f}s)')
        self.ax.axhline(y=lower_threshold, color='green', linestyle='--', alpha=0.5,
                       label=f'Lower Threshold ({self.threshold_percent}% below avg: {lower_threshold:.2f}s)')
        self.ax.axhline(y=avg_interval, color='blue', linestyle='--', alpha=0.5,
                       label=f'Average ({avg_interval:.2f}s)')

        # Calculate and emit statistics
        total_intervals = len(intervals)
        plot_statistics["total_intervals"] = total_intervals

        if intervals_above:
            plot_statistics["avg_interval_above_threshold"] = np.mean(intervals_above)
            plot_statistics["percent_above_threshold"] = (len(intervals_above) / total_intervals) * 100
        if intervals_below:
            plot_statistics["avg_interval_below_threshold"] = np.mean(intervals_below)
            plot_statistics["percent_below_threshold"] = (len(intervals_below) / total_intervals) * 100
        if intervals_within:
            plot_statistics["avg_interval_within_threshold"] = np.mean(intervals_within)
            plot_statistics["percent_within_threshold"] = (len(intervals_within) / total_intervals) * 100

        self.plot_stats_updated.emit(plot_statistics)

        # Set plot title and labels with custom styling
        self.ax.set_title("Time Intervals Between Consecutive Records", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
        self.ax.set_xlabel("Record Time (Iran TZ, UTC+3:30)", fontsize=12, color='#1a3c34')
        self.ax.set_ylabel("Interval (seconds)", fontsize=12, color='#1a3c34')
        self.ax.grid(True, linestyle='--', alpha=0.7, color='#d9d9d9') # Add grid
        self.ax.set_facecolor('#ffffff') # Set background color of the plot area
        self.ax.legend() # Display legend

        # Format x-axis with Iran TZ and rotate labels for readability
        self.ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d %H:%M:%S', tz=IRAN_TZ))
        self.ax.xaxis.set_major_locator(mdates.AutoDateLocator())
        self.ax.tick_params(axis='x', rotation=45, labelright=True, labelsize=10, colors='#1a3c34')
        self.ax.tick_params(axis='y', labelsize=10, colors='#1a3c34')
        # Customize plot borders
        self.ax.spines['top'].set_visible(False)
        self.ax.spines['right'].set_visible(False)
        self.ax.spines['left'].set_color('#1a3c34')
        self.ax.spines['bottom'].set_color('#1a3c34')
        self.fig.tight_layout() # Adjust layout to prevent labels overlapping

        # Initialize the annotation for hover effects (initially hidden)
        self.annotation = self.ax.annotate("", xy=(0, 0), xytext=(10, 10), textcoords="offset points",
                                         bbox=dict(boxstyle="round,pad=0.5", fc="#e9ecef", ec="#1a3c34"),
                                         arrowprops=dict(arrowstyle="->"), fontsize=10, color='#1a3c34')
        self.annotation.set_visible(False)

        # Connect mouse events for hover and click interactions
        self.fig.canvas.mpl_connect("motion_notify_event", self.on_hover)
        self.fig.canvas.mpl_connect("button_press_event", self.on_click)

        self.draw() # Redraw the canvas

    def on_hover(self, event):
        """
        Handles mouse hover events to display annotations with data point details.
        """
        if event.inaxes != self.ax or not self.record_times:
            self.annotation.set_visible(False)
            self.draw()
            return

        xdata = mdates.date2num(self.record_times) # Convert datetime to numeric for calculation
        ydata = np.array(self.intervals)
        if event.xdata is None or event.ydata is None:
            self.annotation.set_visible(False)
            self.draw()
            return

        # Normalize distances to account for different scales on x and y axes
        x_range = max(xdata) - min(xdata) if len(xdata) > 1 else 1
        y_range = max(ydata, default=1) - min(ydata, default=0) if len(ydata) > 1 else 1
        distances = np.sqrt(((xdata - event.xdata) / x_range) ** 2 + ((ydata - event.ydata) / y_range) ** 2)
        closest_idx = np.argmin(distances) # Find the index of the closest data point

        # Show annotation if the mouse is close enough to a data point
        if distances[closest_idx] < 0.05: # Sensitivity threshold
            self.annotation.set_visible(True)
            self.annotation.xy = (xdata[closest_idx], ydata[closest_idx])
            # Format annotation text with Iran TZ
            self.annotation.set_text(
                f"Time: {self.record_times[closest_idx].strftime('%Y-%m-%d %H:%M:%S')} (UTC+3:30)\n"
                f"Interval: {self.intervals[closest_idx]:.2f} seconds"
            )
        else:
            self.annotation.set_visible(False)

        self.draw() # Redraw the canvas to update annotation visibility

    def on_click(self, event):
        """
        Handles mouse click events. If a data point is clicked, opens a dialog
        to show nearby records.
        """
        if event.inaxes != self.ax or not self.record_times or event.button != 1: # Only left clicks
            return

        xdata = mdates.date2num(self.record_times)
        ydata = np.array(self.intervals)
        if event.xdata is None or event.ydata is None:
            return

        # Find closest point similar to on_hover
        x_range = max(xdata) - min(xdata) if len(xdata) > 1 else 1
        y_range = max(ydata, default=1) - min(ydata, default=0) if len(ydata) > 1 else 1
        distances = np.sqrt(((xdata - event.xdata) / x_range) ** 2 + ((ydata - event.ydata) / y_range) ** 2)
        closest_idx = np.argmin(distances)

        if distances[closest_idx] < 0.05: # If a data point was clicked
            # Open RecordsDialog with the relevant records and the index of the clicked point
            # Note: self.records here are the original (time, count) pairs, and closest_idx refers to them.
            dialog = RecordsDialog(self.records, closest_idx, self.main_window)
            dialog.exec() # Show the dialog modally

class PlotWindow(QMainWindow):
    """
    A separate window to display the production data plot and allow
    users to adjust plot settings like the threshold.
    Now also displays plot statistics.
    """
    def __init__(self, record_times, intervals, records, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Time Interval Plot")
        self.setGeometry(150, 150, 1000, 700) # Larger default size for the plot window
        self.record_times = record_times
        self.intervals = intervals
        self.records = records

        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.layout = QVBoxLayout(self.central_widget)

        # Plot statistics label
        self.stats_label = QLabel("Plot Statistics will appear here.")
        self.stats_label.setFont(QFont("Helvetica", 11))
        self.stats_label.setStyleSheet("""
            QLabel {
                background-color: #e9ecef;
                border: 1px solid #d0d4d8;
                border-radius: 6px;
                padding: 10px;
                margin-bottom: 5px; /* Reduced margin */
                color: #1a3c34;
            }
        """)
        self.layout.addWidget(self.stats_label)

        # Create the PlotCanvas and add it to the layout
        self.canvas = PlotCanvas(self.central_widget, records=self.records, main_window=parent) # Pass main window as parent
        self.layout.addWidget(self.canvas)

        # Connect the signal from PlotCanvas to update statistics in this window
        self.canvas.plot_stats_updated.connect(self.update_plot_statistics)

        # Plot settings button
        self.settings_button = QPushButton("Plot Settings")
        self.settings_button.setToolTip("Adjust the outlier threshold for the plot")
        self.settings_button.clicked.connect(self.show_plot_settings)
        self.layout.addWidget(self.settings_button)

        # Adjust stretch factors for layout (more space for canvas)
        self.layout.setStretchFactor(self.stats_label, 1) # Give less space to stats
        self.layout.setStretchFactor(self.canvas, 10) # Give much more space to the plot
        self.layout.setStretchFactor(self.settings_button, 1) # Keep settings button small

        # Initial plot display
        self.canvas.plot(self.record_times, self.intervals, self.records)


    def show_plot_settings(self):
        """
        Opens the PlotSettingsDialog to allow changing the threshold.
        Connects the dialog's signal to update the plot.
        """
        # Create the settings dialog with the current threshold value
        dialog = PlotSettingsDialog(self.canvas.threshold_percent, self)
        # Connect the threshold_changed signal from the dialog to this window's slot
        dialog.threshold_changed.connect(self.update_plot_threshold)
        dialog.exec() # Show the dialog modally

    def update_plot_threshold(self, new_threshold):
        """
        Slot to receive the new threshold from PlotSettingsDialog and redraw the plot.
        """
        self.canvas.threshold_percent = new_threshold
        self.canvas.plot(self.record_times, self.intervals, self.records) # Redraw the plot with the new threshold

    def update_plot_statistics(self, stats):
        """
        Slot to receive plot statistics from PlotCanvas and update the stats_label.
        """
        if not stats or stats["total_intervals"] == 0:
            self.stats_label.setText("Plot Statistics: No intervals to analyze.")
            return

        stats_text = (
            f"<b>📊 Plot Statistics:</b><br>"
            f"<b>Total Intervals:</b> {stats['total_intervals']}<br>"
            f"<b>Above Threshold (Avg:</b> {stats['avg_interval_above_threshold']:.2f}s, <b>%:</b> {stats['percent_above_threshold']:.2f}%)<br>"
            f"<b>Below Threshold (Avg:</b> {stats['avg_interval_below_threshold']:.2f}s, <b>%:</b> {stats['percent_below_threshold']:.2f}%)<br>"
            f"<b>Within Threshold (Avg:</b> {stats['avg_interval_within_threshold']:.2f}s, <b>%:</b> {stats['percent_within_threshold']:.2f}%)"
        )
        self.stats_label.setText(stats_text)

    def resizeEvent(self, event):
        """
        Override resizeEvent to ensure the plot canvas redraws and resizes when the window is resized.
        """
        super().resizeEvent(event)
        # Manually trigger a redraw of the canvas if it exists
        if self.canvas:
            # Update the matplotlib figure size based on the new widget size
            # Convert pixels to inches (dpi is needed)
            size = self.canvas.size()
            new_width_inches = size.width() / self.canvas.figure.dpi
            new_height_inches = size.height() / self.canvas.figure.dpi
            self.canvas.figure.set_size_inches(new_width_inches, new_height_inches, forward=True)
            self.canvas.draw()


class InfluxDBApp(QMainWindow):
    """
    Main application window for querying and analyzing InfluxDB production data.
    Allows selection of device, date range, and displays summary and a plot.
    """
    def __init__(self):
        super().__init__()
        self.setWindowTitle("InfluxDB Production Data Analyzer")
        self.setGeometry(100, 100, 900, 700)
        self.setMinimumSize(600, 400)

        # Main widget and layout
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.main_layout = QVBoxLayout(self.central_widget)
        self.main_layout.setContentsMargins(15, 15, 15, 15)
        self.main_layout.setSpacing(12)

        # Apply modern style sheet for a cleaner look
        self.setStyleSheet("""
            QMainWindow, QWidget { background-color: #f0f4f8; } /* Light grey background */
            QGroupBox {
                font: bold 13px Helvetica;
                border: 1px solid #d0d4d8; /* Light border */
                border-radius: 6px;
                margin-top: 12px;
                padding: 12px;
                background-color: #ffffff; /* White background for group boxes */
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 6px;
                color: #1a3c34; /* Dark green for titles */
            }
            QLabel {
                font: 11px Helvetica;
                color: #1a3c34;
                background-color: transparent;
            }
            QLineEdit, QDateEdit {
                padding: 6px;
                border: 1px solid #b0b4b8; /* Grey border */
                border-radius: 4px;
                background-color: #ffffff;
                color: #1a3c34;
            }
            QComboBox {
                padding: 6px;
                border: 1px solid #b0b4b8;
                border-radius: 4px;
                background-color: #ffffff;
                color: #000000;
            }
            QComboBox QAbstractItemView {
                color: #000000;
                selection-background-color: #2a9d8f; /* Teal for selection */
                selection-color: #ffffff;
            }
            QPushButton {
                background-color: #2a9d8f; /* Teal button background */
                color: #ffffff; /* White text */
                padding: 8px;
                border-radius: 4px;
                font: bold 11px Helvetica;
            }
            QPushButton:hover { background-color: #21867a; } /* Slightly darker teal on hover */
            #summaryLabel { /* Specific style for the summary label */
                background-color: #ffffff;
                border: 1px solid #d0d4d8;
                border-radius: 6px;
                padding: 12px;
                font: 12px Helvetica;
                color: #1a3c34;
            }
        """)

        # Query Parameters Group
        query_group = QGroupBox("Query Parameters")
        query_layout = QGridLayout()
        query_layout.setContentsMargins(10, 10, 10, 10)
        query_layout.setSpacing(10)
        query_group.setLayout(query_layout)

        # Device ID selection
        self.device_label = QLabel("Device ID:")
        self.device_label.setToolTip("Select the unique identifier for the device")
        self.device_combo = QComboBox()
        self.device_combo.setToolTip("Select the unique identifier for the device")
        # Connect here, but populate_device_ids will be called later, which triggers initial run
        self.device_combo.currentIndexChanged.connect(self.run_query)
        query_layout.addWidget(self.device_label, 0, 0)
        query_layout.addWidget(self.device_combo, 0, 1, 1, 3) # Span 3 columns for combobox

        # Start Date/Time
        self.start_date_label = QLabel("Start Date:")
        self.start_date_label.setToolTip("Select the start date (YYYY-MM-DD)")
        self.start_date_entry = QDateEdit()
        self.start_date_entry.setDisplayFormat("yyyy-MM-dd")
        self.start_date_entry.setDate(QDate.fromString(
            (datetime.now(IRAN_TZ) - timedelta(hours=1)).strftime("%Y-%m-%d"), "yyyy-MM-dd"))
        self.start_time_label = QLabel("Time:")
        self.start_time_label.setToolTip("Enter the start time (HH:MM:SS)")
        self.start_time_entry = QLineEdit((datetime.now(IRAN_TZ) - timedelta(hours=1)).strftime("%H:%M:%S"))
        self.start_time_entry.setMaximumWidth(80)

        query_layout.addWidget(self.start_date_label, 1, 0)
        query_layout.addWidget(self.start_date_entry, 1, 1)
        query_layout.addWidget(self.start_time_label, 1, 2)
        query_layout.addWidget(self.start_time_entry, 1, 3)

        # End Date/Time
        self.end_date_label = QLabel("End Date:")
        self.end_date_label.setToolTip("Select the end date (YYYY-MM-DD)")
        self.end_date_entry = QDateEdit()
        self.end_date_entry.setDisplayFormat("yyyy-MM-dd")
        self.end_date_entry.setDate(QDate.fromString(
            datetime.now(IRAN_TZ).strftime("%Y-%m-%d"), "yyyy-MM-dd"))
        self.end_time_label = QLabel("Time:")
        self.end_time_label.setToolTip("Enter the end time (HH:MM:S)")
        self.end_time_entry = QLineEdit(datetime.now(IRAN_TZ).strftime("%H:%M:%S"))
        self.end_time_entry.setMaximumWidth(80)

        query_layout.addWidget(self.end_date_label, 2, 0)
        query_layout.addWidget(self.end_date_entry, 2, 1)
        query_layout.addWidget(self.end_time_label, 2, 2)
        query_layout.addWidget(self.end_time_entry, 2, 3)


        # Action Buttons Layout (Run Query, Export to Excel, Show Plot)
        buttons_layout = QHBoxLayout()
        self.run_button = QPushButton("Run Query")
        self.run_button.setToolTip("Execute the query with the specified parameters and show summary")
        self.run_button.clicked.connect(self.run_query)
        self.export_button = QPushButton("Export to Excel")
        self.export_button.setToolTip("Export query results to an Excel file")
        self.export_button.clicked.connect(self.export_to_excel)
        self.export_button.setEnabled(False) # Disabled until data is fetched
        self.show_plot_button = QPushButton("Show Plot")
        self.show_plot_button.setToolTip("Open a new window to display the time interval plot")
        self.show_plot_button.clicked.connect(self.show_plot_window)
        self.show_plot_button.setEnabled(False) # Disabled until data is fetched

        buttons_layout.addWidget(self.run_button)
        buttons_layout.addWidget(self.export_button)
        buttons_layout.addWidget(self.show_plot_button)
        query_layout.addLayout(buttons_layout, 3, 0, 1, 4) # Span all 4 columns for buttons

        self.main_layout.addWidget(query_group)

        # Data Management Group
        data_mgmt_group = QGroupBox("Data Management")
        data_mgmt_layout = QHBoxLayout()
        data_mgmt_layout.setSpacing(10)
        data_mgmt_layout.setContentsMargins(0, 0, 0, 0) # Removed internal margins of groupbox

        self.delete_all_button = QPushButton("Delete All Device Records")
        self.delete_all_button.setToolTip("Permanently delete ALL production records for the selected device.")
        self.delete_all_button.clicked.connect(self.delete_all_device_records)
        self.delete_all_button.setEnabled(False) # Enabled after a device is selected
        data_mgmt_layout.addWidget(self.delete_all_button)

        self.delete_range_button = QPushButton("Delete Device Records in Range")
        self.delete_range_button.setToolTip("Permanently delete production records for the selected device within the specified date/time range.")
        self.delete_range_button.clicked.connect(self.delete_device_records_in_range)
        self.delete_range_button.setEnabled(False) # Enabled after a device is selected
        data_mgmt_layout.addWidget(self.delete_range_button)

        data_mgmt_group.setLayout(data_mgmt_layout)
        self.main_layout.addWidget(data_mgmt_group)


        # Query Results Group
        results_group = QGroupBox("Query Results")
        results_layout = QVBoxLayout()
        results_layout.setSpacing(10)
        results_group.setLayout(results_layout)

        # Summary Label
        self.summary_label = QLabel("")
        self.summary_label.setObjectName("summaryLabel") # For specific styling
        self.summary_label.setWordWrap(True)
        self.summary_label.setFont(QFont("Helvetica", 12))
        results_layout.addWidget(self.summary_label)

        # Startup Records Button
        self.startup_button = QPushButton("Show Startup Records")
        self.startup_button.setToolTip("Show all device startup records in the selected time range")
        self.startup_button.clicked.connect(self.show_startup_records)
        self.startup_button.setEnabled(False)  # Disabled until query runs
        results_layout.addWidget(self.startup_button)
        self.main_layout.addWidget(results_group)

        # Placeholder for plot window reference
        self.plot_window = None

        # Store data needed for queries and plots
        self.device_info = {}
        self.records = []
        self.intervals = []
        self.record_times = []

        # Adjust stretch factors for overall layout
        # Query parameters group
        self.main_layout.setStretchFactor(query_group, 0) # Make it take minimum necessary space
        # Data management group
        self.main_layout.setStretchFactor(data_mgmt_group, 0) # Make it take minimum necessary space
        # Query results group (summary + startup button)
        self.main_layout.setStretchFactor(results_group, 1) # Allow it to stretch

        # Ensure populate_device_ids is called AFTER all UI elements are initialized,
        # as it triggers the initial run_query which depends on these elements.
        self.populate_device_ids()


    def populate_device_ids(self):
        """
        Queries InfluxDB for unique device IDs from 'production' measurement
        and their latest 'device_info' (IP and RSSI).
        Populates the device ID combobox.
        """
        flux_query_devices = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
          |> range(start: -30d) // Look back 30 days for active devices
          |> filter(fn: (r) => r["_measurement"] == "{MEASUREMENT}")
          |> filter(fn: (r) => r["_field"] == "{FIELD_NAME}")
          |> group(columns: ["device"])
          |> distinct(column: "device")
        '''

        flux_query_info = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
          |> range(start: -30d) // Look back 30 days for device info
          |> filter(fn: (r) => r["_measurement"] == "{DEVICE_INFO_MEASUREMENT}")
          |> filter(fn: (r) => r["_field"] == "ip" or r["_field"] == "rssi")
          |> last() // Get the latest info for each device
          |> pivot(rowKey: ["_time", "device"], columnKey: ["_field"], valueColumn: "_value")
          |> group(columns: ["device"])
        '''

        client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
        query_api = client.query_api()

        try:
            # Fetch device IDs
            result = query_api.query(org=INFLUXDB_ORG, query=flux_query_devices)
            device_ids = []
            for table in result:
                for record in table.records:
                    device_id = record.values.get("device")
                    if device_id:
                        device_ids.append(device_id)

            # Fetch device info
            result_info = query_api.query(org=INFLUXDB_ORG, query=flux_query_info)
            self.device_info = {}
            for table in result_info:
                for record in table.records:
                    device_id = record.values.get("device")
                    ip_address = record.values.get("ip")
                    rssi = record.values.get("rssi")
                    startup_time = record.get_time() # This is the _time of the last info record
                    if device_id and ip_address and startup_time:
                        rssi = int(rssi) if rssi is not None else -100 # Default RSSI if none
                        self.device_info[device_id] = (startup_time.astimezone(IRAN_TZ), ip_address, rssi)

            # Sort device IDs and populate the combobox
            device_ids.sort()
            self.device_combo.clear() # Clear existing items before adding new ones
            if device_ids:
                self.device_combo.addItems(device_ids)
                self.device_combo.setCurrentText(device_ids[0])
                self.device_combo.setEnabled(True)
                self.delete_all_button.setEnabled(True) # Enable delete buttons if devices found
                self.delete_range_button.setEnabled(True)
                # After populating, trigger a query run if there are devices
                # This is handled by currentIndexChanged.connect(self.run_query)
            else:
                self.device_combo.addItem("No devices found")
                self.device_combo.setEnabled(False)
                self.delete_all_button.setEnabled(False)
                self.delete_range_button.setEnabled(False)
                QMessageBox.warning(self, "No Devices", "No device IDs found in the database for the last 30 days.")

        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to fetch device info: {e}")
            self.device_combo.clear() # Clear in case of error
            self.device_combo.addItem("Error fetching devices")
            self.device_combo.setEnabled(False)
            self.delete_all_button.setEnabled(False)
            self.delete_range_button.setEnabled(False)

        finally:
            client.close()

    def show_startup_records(self):
        """
        Queries and displays all device startup records (device_info measurement)
        for the selected device within the specified time range in a new dialog.
        """
        device_id = self.device_combo.currentText().strip()
        if not device_id or device_id == "No devices found" or device_id == "Error fetching devices":
            QMessageBox.warning(self, "No Device Selected", "Please select a valid device ID first.")
            return

        start_date = self.start_date_entry.date().toString("yyyy-MM-dd")
        start_time = self.start_time_entry.text().strip()
        end_date = self.end_date_entry.date().toString("yyyy-MM-dd")
        end_time = self.end_time_entry.text().strip()

        try:
            # Combine date and time inputs and convert to UTC
            start_dt_str = f"{start_date}T{start_time}"
            start_time_iran = datetime.strptime(start_dt_str, "%Y-%m-%dT%H:%M:%S").replace(tzinfo=IRAN_TZ)
            end_dt_str = f"{end_date}T{end_time}"
            end_time_iran = datetime.strptime(end_dt_str, "%Y-%m-%dT%H:%M:%S").replace(tzinfo=IRAN_TZ)
            start_time_utc = start_time_iran.astimezone(UTC_TZ)
            end_time_utc = end_time_iran.astimezone(UTC_TZ)
        except ValueError:
            QMessageBox.critical(self, "Invalid Input", "Please enter valid dates and times in the format shown.")
            return

        # Clean and escape device_id for safe inclusion in Flux query
        cleaned_device_id = device_id.encode('ascii', 'ignore').decode('ascii')
        escaped_device_id = escape_flux_string(cleaned_device_id)

        flux_query = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
          |> range(start: {start_time_utc.isoformat()}, stop: {end_time_utc.isoformat()})
          |> filter(fn: (r) => r["_measurement"] == "{DEVICE_INFO_MEASUREMENT}")
          |> filter(fn: (r) => r["_field"] == "ip" or r["_field"] == "rssi")
          |> filter(fn: (r) => r["device"] == "{escaped_device_id}")
          |> pivot(rowKey: ["_time", "device"], columnKey: ["_field"], valueColumn: "_value")
          |> sort(columns: ["_time"])
        '''

        client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
        query_api = client.query_api()

        try:
            result = query_api.query(org=INFLUXDB_ORG, query=flux_query)
            startup_records = []
            for table in result:
                for record in table.records:
                    time = record.get_time()
                    ip = record.values.get("ip", "N/A")
                    rssi = int(record.values.get("rssi", -100))
                    startup_records.append((time, ip, rssi))

            if not startup_records:
                QMessageBox.information(self, "No Startup Records", "No startup records found in the specified time range for this device.")
                return

            dialog = StartupRecordsDialog(startup_records, self)
            dialog.exec() # Show the dialog

        except Exception as e:
            QMessageBox.critical(self, "Query Error", f"Error querying startup records: {e}")
        finally:
            client.close()

    def run_query(self):
        """
        Executes the InfluxDB query based on user inputs, calculates intervals,
        updates the summary, and enables plot and export buttons.
        This method is also called automatically when the device selection changes.
        """
        device_id = self.device_combo.currentText().strip()
        if not device_id or device_id == "No devices found" or device_id == "Error fetching devices":
            self.summary_label.setText("Please select a valid device ID to run a query.")
            self.records = []
            self.intervals = []
            self.record_times = []
            self.export_button.setEnabled(False)
            self.show_plot_button.setEnabled(False)
            self.startup_button.setEnabled(False)
            self.delete_all_button.setEnabled(False)
            self.delete_range_button.setEnabled(False)
            if self.plot_window and self.plot_window.isVisible():
                self.plot_window.close()
                self.plot_window = None
            return

        # Ensure date/time entries are initialized before accessing them
        start_date = self.start_date_entry.date().toString("yyyy-MM-dd")
        start_time = self.start_time_entry.text().strip()
        end_date = self.end_date_entry.date().toString("yyyy-MM-dd")
        end_time = self.end_time_entry.text().strip()


        # Clear previous results and disable buttons
        self.summary_label.setText("")
        self.records = []
        self.intervals = []
        self.record_times = []
        self.export_button.setEnabled(False)
        self.show_plot_button.setEnabled(False)
        self.startup_button.setEnabled(False)
        self.delete_all_button.setEnabled(True) # Keep enabled since device is selected
        self.delete_range_button.setEnabled(True) # Keep enabled since device is selected

        # Close existing plot window if open
        if self.plot_window and self.plot_window.isVisible():
            self.plot_window.close()
            self.plot_window = None

        # Validate and convert date/time inputs
        try:
            start_dt_str = f"{start_date}T{start_time}"
            start_time_iran = datetime.strptime(start_dt_str, "%Y-%m-%dT%H:%M:%S").replace(tzinfo=IRAN_TZ)
            end_dt_str = f"{end_date}T{end_time}"
            end_time_iran = datetime.strptime(end_dt_str, "%Y-%m-%dT%H:%M:%S").replace(tzinfo=IRAN_TZ)

            if end_time_iran <= start_time_iran:
                QMessageBox.critical(self, "Invalid Time Range", "End time must be after start time.")
                return

            start_time_utc = start_time_iran.astimezone(UTC_TZ)
            end_time_utc = end_time_iran.astimezone(UTC_TZ)

        except ValueError:
            QMessageBox.critical(self, "Invalid Input", "Please enter valid dates and times in the format shown.")
            return

        # Clean and escape device_id for safe inclusion in Flux query
        cleaned_device_id = device_id.encode('ascii', 'ignore').decode('ascii')
        escaped_device_id = escape_flux_string(cleaned_device_id)

        flux_query = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
          |> range(start: {start_time_utc.isoformat()}, stop: {end_time_utc.isoformat()})
          |> filter(fn: (r) => r["_measurement"] == "{MEASUREMENT}")
          |> filter(fn: (r) => r["_field"] == "{FIELD_NAME}")
          |> filter(fn: (r) => r["device"] == "{escaped_device_id}")
          |> sort(columns: ["_time"])
        '''

        client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
        query_api = client.query_api()

        try:
            result = query_api.query(org=INFLUXDB_ORG, query=flux_query)
            self.records = []
            count_sum = 0

            # Process query results
            for table in result:
                for record in table.records:
                    count = record.get_value()
                    if count != 0:  # Exclude records with count == 0 for interval calculation
                        self.records.append((record.get_time(), count))
                        count_sum += count

            if not self.records:
                self.summary_label.setText("No non-zero count records found in the specified time range for this device.")
                return

            self.records.sort(key=lambda x: x[0]) # Ensure records are sorted by time
            start_record_time, start_count = self.records[0]
            end_record_time, end_count = self.records[-1]
            duration = end_record_time - start_record_time

            # Calculate time intervals between consecutive non-zero records
            if len(self.records) > 1:
                self.intervals = [(self.records[i+1][0] - self.records[i][0]).total_seconds() for i in range(len(self.records)-1)]
                self.record_times = [self.records[i+1][0].astimezone(IRAN_TZ) for i in range(len(self.records)-1)]
            else:
                self.intervals = []
                self.record_times = []


            # Calculate summary statistics
            avg_interval = np.mean(self.intervals) if self.intervals else 0

            # Display summary information
            start_record_time_iran = start_record_time.astimezone(IRAN_TZ)
            end_record_time_iran = end_record_time.astimezone(IRAN_TZ)
            summary_text = (
                f"<b>📊 Total Count (non-zero):</b> {count_sum}<br>"
                f"<b>⏱ Duration of Records:</b> {duration}<br>"
                f"<b>🕒 First Record Time (Iran TZ, UTC+3:30):</b> {start_record_time_iran.strftime('%Y-%m-%d %H:%M:%S')}<br>"
                f"<b>🕔 Last Record Time (Iran TZ, UTC+3:30):</b> {end_record_time_iran.strftime('%Y-%m-%d %H:%M:%S')}<br>"
            )
            if device_id in self.device_info:
                _, ip_address, rssi = self.device_info[device_id]
                summary_text += (
                    f"<b>🌐 IP Address:</b> {ip_address}<br>"
                    f"<b>📶 Signal Quality:</b> {get_signal_quality(rssi)} ({rssi} dBm)<br>"
                )
            if len(self.records) > 1:
                summary_text += f"<b>⏲ Average Interval:</b> {avg_interval:.2f} seconds<br>"
                summary_text += f"<b>ℹ Total Intervals Measured:</b> {len(self.intervals)}"
            else:
                summary_text += "<b>ℹ Note:</b> Only one non-zero count record found — interval analysis not applicable."

            self.summary_label.setText(summary_text)

            # Enable buttons now that data is available
            self.export_button.setEnabled(True)
            self.show_plot_button.setEnabled(True)
            self.startup_button.setEnabled(True)

        except Exception as e:
            QMessageBox.critical(self, "Query Error", f"Error querying InfluxDB: {e}")
            self.export_button.setEnabled(False)
            self.show_plot_button.setEnabled(False)
            self.startup_button.setEnabled(False)

        finally:
            client.close()

    def show_plot_window(self):
        """
        Creates and shows a new PlotWindow if data is available.
        """
        if not self.records or not self.intervals:
            QMessageBox.information(self, "No Plot Data", "Run a query first to get data for the plot.")
            return

        # Create and show the plot window, passing the data
        # Pass self (the main window) as the parent so dialogs launched from plot can access main window
        self.plot_window = PlotWindow(self.record_times, self.intervals, self.records, parent=self)
        self.plot_window.show()

    def export_to_excel(self):
        """
        Exports the fetched production data to an Excel file.
        Includes device info if available.
        """
        if not self.records:
            QMessageBox.warning(self, "No Data", "No data available to export.")
            return

        device_id = self.device_combo.currentText().strip()
        data = {
            "Time (Iran TZ, UTC+3:30)": [t.astimezone(IRAN_TZ).strftime('%Y-%m-%d %H:%M:%S') for t, _ in self.records],
            "Count": [v for _, v in self.records]
        }
        if device_id in self.device_info:
            # Add IP Address and Signal Quality to the first row of the Excel sheet
            _, ip_address, rssi = self.device_info[device_id]
            data["IP Address"] = [ip_address] + [""] * (len(self.records) - 1)
            data["Signal Quality"] = [f"{get_signal_quality(rssi)} ({rssi} dBm)"] + [""] * (len(self.records) - 1)

        df = pd.DataFrame(data)
        timestamp = datetime.now(IRAN_TZ).strftime("%Y%m%d_%H%M%S")
        filename = f"production_data_{timestamp}.xlsx"
        try:
            df.to_excel(filename, index=False)
            QMessageBox.information(self, "Export Successful", f"Data exported to {os.path.abspath(filename)}")
        except Exception as e:
            QMessageBox.critical(self, "Export Error", f"Failed to export data: {e}")

    def delete_all_device_records(self):
        """
        Deletes all production records for the currently selected device.
        Requires user confirmation.
        """
        device_id = self.device_combo.currentText().strip()
        if not device_id or device_id == "No devices found" or device_id == "Error fetching devices":
            QMessageBox.warning(self, "No Device Selected", "Please select a valid device ID to delete records.")
            return

        reply = QMessageBox.question(self, 'Confirm Deletion',
                                     f"Are you sure you want to permanently delete ALL production records for device '{device_id}'?\n"
                                     "This action cannot be undone.",
                                     QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                                     QMessageBox.StandardButton.No)

        if reply == QMessageBox.StandardButton.Yes:
            client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
            delete_api = client.delete_api()

            try:
                # Clean and escape device_id for safe inclusion in Flux query
                cleaned_device_id = device_id.encode('ascii', 'ignore').decode('ascii')
                escaped_device_id = escape_flux_string(cleaned_device_id)

                # Delete all data for the specific device in the 'manufacture' bucket
                start_time = datetime(1970, 1, 1, 0, 0, 0, tzinfo=UTC_TZ) # Epoch start
                end_time = datetime.now(UTC_TZ) # Current time in UTC

                # Delete from 'production' measurement
                delete_api.delete(
                    start=start_time,
                    stop=end_time,
                    predicate=f'r._measurement == "{MEASUREMENT}" and r.device == "{escaped_device_id}"',
                    bucket=INFLUXDB_BUCKET,
                    org=INFLUXDB_ORG
                )
                # Also delete from 'device_info' measurement
                delete_api.delete(
                    start=start_time,
                    stop=end_time,
                    predicate=f'r._measurement == "{DEVICE_INFO_MEASUREMENT}" and r.device == "{escaped_device_id}"',
                    bucket=INFLUXDB_BUCKET,
                    org=INFLUXDB_ORG
                )

                QMessageBox.information(self, "Deletion Successful", f"All records for device '{device_id}' have been deleted.")
                # After deletion, re-populate device IDs and re-run query
                self.populate_device_ids() # This will internally call run_query if devices exist
            except Exception as e:
                QMessageBox.critical(self, "Deletion Error", f"Failed to delete records: {e}")
            finally:
                client.close()

    def delete_device_records_in_range(self):
        """
        Deletes production records for the selected device within the specified date/time range.
        Requires user confirmation.
        """
        device_id = self.device_combo.currentText().strip()
        if not device_id or device_id == "No devices found" or device_id == "Error fetching devices":
            QMessageBox.warning(self, "No Device Selected", "Please select a valid device ID first.")
            return

        start_date = self.start_date_entry.date().toString("yyyy-MM-dd")
        start_time = self.start_time_entry.text().strip()
        end_date = self.end_date_entry.date().toString("yyyy-MM-dd")
        end_time = self.end_time_entry.text().strip()

        try:
            start_dt_str = f"{start_date}T{start_time}"
            start_time_iran = datetime.strptime(start_dt_str, "%Y-%m-%dT%H:%M:%S").replace(tzinfo=IRAN_TZ)
            end_dt_str = f"{end_date}T{end_time}"
            end_time_iran = datetime.strptime(end_dt_str, "%Y-%m-%dT%H:%M:%S").replace(tzinfo=IRAN_TZ)

            if end_time_iran <= start_time_iran:
                QMessageBox.critical(self, "Invalid Time Range", "End time must be after start time for deletion.")
                return

            start_time_utc = start_time_iran.astimezone(UTC_TZ)
            end_time_utc = end_time_iran.astimezone(UTC_TZ)

        except ValueError:
            QMessageBox.critical(self, "Invalid Input", "Please enter valid dates and times in the format shown for the deletion range.")
            return

        reply = QMessageBox.question(self, 'Confirm Deletion',
                                     f"Are you sure you want to permanently delete production records for device '{device_id}' "
                                     f"from {start_time_iran.strftime('%Y-%m-%d %H:%M:%S')} to {end_time_iran.strftime('%Y-%m-%d %H:%M:%S')} (Iran TZ)?\n"
                                     "This action cannot be undone.",
                                     QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                                     QMessageBox.StandardButton.No)

        if reply == QMessageBox.StandardButton.Yes:
            client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
            delete_api = client.delete_api()

            try:
                # Clean and escape device_id for safe inclusion in Flux query
                cleaned_device_id = device_id.encode('ascii', 'ignore').decode('ascii')
                escaped_device_id = escape_flux_string(cleaned_device_id)

                # Delete from 'production' measurement within the specified range
                delete_api.delete(
                    start=start_time_utc,
                    stop=end_time_utc,
                    predicate=f'r._measurement == "{MEASUREMENT}" and r.device == "{escaped_device_id}"',
                    bucket=INFLUXDB_BUCKET,
                    org=INFLUXDB_ORG
                )
                # Also delete from 'device_info' measurement within the specified range
                delete_api.delete(
                    start=start_time_utc,
                    stop=end_time_utc,
                    predicate=f'r._measurement == "{DEVICE_INFO_MEASUREMENT}" and r.device == "{escaped_device_id}"',
                    bucket=INFLUXDB_BUCKET,
                    org=INFLUXDB_ORG
                )
                QMessageBox.information(self, "Deletion Successful", f"Records for device '{device_id}' in the specified range have been deleted.")
                self.run_query() # Refresh data after deletion
            except Exception as e:
                QMessageBox.critical(self, "Deletion Error", f"Failed to delete records: {e}")
            finally:
                client.close()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = InfluxDBApp()
    window.show()
    sys.exit(app.exec())

