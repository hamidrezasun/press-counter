import sys
from datetime import datetime, timedelta, timezone
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                            QLabel, QLineEdit, QPushButton, QGroupBox, QDateEdit, QMessageBox,
                            QComboBox, QTableWidget, QTableWidgetItem, QDialog, QGridLayout,
                            QCheckBox, QCalendarWidget)
from PyQt6.QtCore import Qt, QDate, QTimer
from PyQt6.QtGui import QFont, QTextCharFormat, QColor
from influxdb_client import InfluxDBClient
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import matplotlib.dates as mdates
import numpy as np
import pandas as pd
import seaborn as sns
from sklearn.ensemble import IsolationForest

# --- Configuration ---
INFLUXDB_URL = "http://192.168.1.37:8086"
INFLUXDB_TOKEN = "_2B1s0cJGXznc9rXE41mxOusyn159HAAsMyGlLxiFkZd0V8dJKztKYVEPNeKGQSgc0gulh-vhnhn6ONcCr3lBA=="
INFLUXDB_ORG = "tooswasher"
INFLUXDB_BUCKET = "manufacture"
FIELD_NAME = "count"
PULSE_DURATION_FIELD = "pulse_duration_ms"
SEND_DURATION_FIELD = "send_duration_ms"
MEASUREMENT = "production"
RFID_MEASUREMENT = "rfid_tag"

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
    """Escapes a string for safe inclusion within a Flux query string literal."""
    if s is None:
        return ""
    s_str = str(s)
    return s_str.replace('\\', '\\\\').replace('"', '\\"')

class HolidaySettingsDialog(QDialog):
    """Dialog for setting holidays or non-working days with toggle functionality."""
    def __init__(self, holidays=None, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Holiday/Non-Working Days Settings")
        self.setGeometry(200, 200, 400, 400)
        self.holidays = holidays.copy() if holidays else set()
        self.setStyleSheet("""
            QDialog {
                background-color: #f0f4f8;
                color: #1a3c34;
            }
            QLabel {
                color: #1a3c34;
                background-color: transparent;
            }
            QCheckBox {
                color: #1a3c34;
            }
            QPushButton {
                background-color: #2a9d8f;
                color: #ffffff;
                padding: 8px;
                border-radius: 4px;
                font: bold 11px Helvetica;
            }
            QPushButton:hover {
                background-color: #21867a;
            }
            QCalendarWidget {
                background-color: #ffffff;
                color: #1a3c34;
            }
            QCalendarWidget QAbstractItemView:enabled {
                color: #1a3c34;
                background-color: #ffffff;
                selection-background-color: #2a9d8f;
                selection-color: #ffffff;
            }
        """)
        layout = QVBoxLayout(self)

        self.weekend_check = QCheckBox("Exclude Weekends (Friday)")
        self.weekend_check.setChecked(True)
        layout.addWidget(self.weekend_check)

        self.holiday_label = QLabel("Select Holidays (click to toggle):")
        layout.addWidget(self.holiday_label)

        self.calendar = QCalendarWidget()
        self.calendar.setSelectedDate(QDate.currentDate())
        self.calendar.clicked.connect(self.update_holidays)
        layout.addWidget(self.calendar)

        self.holiday_list = QLabel("Selected Holidays: None")
        layout.addWidget(self.holiday_list)

        button_layout = QHBoxLayout()
        self.apply_button = QPushButton("Apply")
        self.apply_button.clicked.connect(self.accept)
        self.cancel_button = QPushButton("Cancel")
        self.cancel_button.clicked.connect(self.reject)
        button_layout.addWidget(self.apply_button)
        button_layout.addWidget(self.cancel_button)
        layout.addLayout(button_layout)

        self.update_holiday_highlights()

    def update_holidays(self):
        """Toggle holiday selection and update highlights."""
        selected_date = self.calendar.selectedDate()
        if not selected_date.isValid():
            return
        date_str = selected_date.toString("yyyy-MM-dd")
        if date_str in self.holidays:
            self.holidays.remove(date_str)
        else:
            self.holidays.add(date_str)
        self.update_holiday_highlights()
        holiday_text = "Selected Holidays: " + ", ".join(sorted(self.holidays)) if self.holidays else "Selected Holidays: None"
        self.holiday_list.setText(holiday_text)

    def update_holiday_highlights(self):
        """Highlight selected holidays in blue on the calendar."""
        format = QTextCharFormat()
        format.setBackground(QColor("#2a9d8f"))
        format.setForeground(QColor("#ffffff"))
        default_format = QTextCharFormat()
        default_format.setBackground(QColor("#ffffff"))
        default_format.setForeground(QColor("#1a3c34"))

        for year in range(2000, 2030):
            for month in range(1, 13):
                for day in range(1, 32):
                    date = QDate(year, month, day)
                    if date.isValid():
                        self.calendar.setDateTextFormat(date, default_format)

        for date_str in self.holidays:
            try:
                date = QDate.fromString(date_str, "yyyy-MM-dd")
                if date.isValid():
                    self.calendar.setDateTextFormat(date, format)
            except:
                continue

    def get_holidays(self):
        return self.holidays, self.weekend_check.isChecked()

class RecordsDialog(QDialog):
    """Dialog to display nearby production records for a selected point on the plot."""
    def __init__(self, records, selected_idx, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Nearby Records")
        self.setGeometry(200, 200, 800, 400)
        layout = QVBoxLayout(self)

        self.table = QTableWidget()
        self.table.setRowCount(21)
        self.table.setColumnCount(4)
        self.table.setHorizontalHeaderLabels(["Time (Iran TZ, UTC+3:30)", "Count", "Pulse Duration (ms)", "Send Duration (ms)"])
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.setStyleSheet("QTableWidget { color: #000000; }")
        layout.addWidget(self.table)

        start_idx = max(0, selected_idx - 10)
        end_idx = min(len(records), selected_idx + 11)
        for i, idx in enumerate(range(start_idx, end_idx)):
            if idx < len(records):
                time, count, pulse_duration, send_duration = records[idx]
                time_item = QTableWidgetItem(time.astimezone(IRAN_TZ).strftime('%Y-%m-%d %H:%M:%S'))
                count_item = QTableWidgetItem(str(count))
                pulse_item = QTableWidgetItem(str(pulse_duration))
                send_item = QTableWidgetItem(str(send_duration))
                if idx == selected_idx:
                    time_item.setBackground(Qt.GlobalColor.yellow)
                    count_item.setBackground(Qt.GlobalColor.yellow)
                    pulse_item.setBackground(Qt.GlobalColor.yellow)
                    send_item.setBackground(Qt.GlobalColor.yellow)
                self.table.setItem(i, 0, time_item)
                self.table.setItem(i, 1, count_item)
                self.table.setItem(i, 2, pulse_item)
                self.table.setItem(i, 3, send_item)
        self.table.resizeColumnsToContents()

class RFIDRecordsDialog(QDialog):
    """Dialog to display RFID tag records."""
    def __init__(self, rfid_records, parent=None):
        super().__init__(parent)
        self.setWindowTitle("RFID Tag Records")
        self.setGeometry(200, 200, 600, 400)
        layout = QVBoxLayout(self)

        self.table = QTableWidget()
        self.table.setRowCount(len(rfid_records))
        self.table.setColumnCount(2)
        self.table.setHorizontalHeaderLabels(["Time (Iran TZ, UTC+3:30)", "RFID UID"])
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.setStyleSheet("QTableWidget { color: #000000; }")
        layout.addWidget(self.table)

        for i, (time, uid) in enumerate(rfid_records):
            time_item = QTableWidgetItem(time.astimezone(IRAN_TZ).strftime('%Y-%m-%d %H:%M:%S'))
            uid_item = QTableWidgetItem(uid)
            self.table.setItem(i, 0, time_item)
            self.table.setItem(i, 1, uid_item)
        self.table.resizeColumnsToContents()

class PlotSettingsDialog(QDialog):
    """Dialog for adjusting plot settings."""
    def __init__(self, current_threshold, exclude_anomalies, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Plot Settings")
        self.setFixedSize(300, 150)
        layout = QVBoxLayout(self)

        threshold_layout = QHBoxLayout()
        self.threshold_label = QLabel("Threshold (%):")
        self.threshold_entry = QLineEdit(str(current_threshold))
        self.threshold_entry.setToolTip("Percentage above/below average for highlighting outliers")
        self.threshold_entry.setMaximumWidth(60)
        threshold_layout.addWidget(self.threshold_label)
        threshold_layout.addWidget(self.threshold_entry)
        threshold_layout.addStretch()
        layout.addLayout(threshold_layout)

        self.exclude_anomalies_check = QCheckBox("Exclude Anomalous Pulse Durations from Calculations")
        self.exclude_anomalies_check.setChecked(exclude_anomalies)
        layout.addWidget(self.exclude_anomalies_check)

        apply_button = QPushButton("Apply")
        apply_button.clicked.connect(self.accept)
        layout.addWidget(apply_button)

    def get_settings(self):
        try:
            threshold = float(self.threshold_entry.text().strip())
            if threshold < 0:
                raise ValueError("Threshold must be non-negative")
            return threshold, self.exclude_anomalies_check.isChecked()
        except ValueError:
            QMessageBox.warning(self, "Invalid Input", "Please enter a valid non-negative percentage for the threshold.")
            return None, None

class HeatmapDialog(QDialog):
    """Dialog to display interactive heatmap with clickable cells for date range."""
    def __init__(self, pivot_table, records, start_date, end_date, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Production Heatmap")
        self.setGeometry(200, 200, 1000, 600)
        self.pivot_table = pivot_table
        self.records = records
        self.start_date = start_date
        self.end_date = end_date
        layout = QVBoxLayout(self)

        self.canvas = FigureCanvas(Figure(figsize=(10, 6)))
        self.ax = self.canvas.figure.add_subplot(111)
        layout.addWidget(self.canvas)

        button_layout = QHBoxLayout()
        self.date_plot_button = QPushButton("Show Date Plot")
        self.date_plot_button.clicked.connect(self.show_date_plot)
        self.hour_plot_button = QPushButton("Show Hour Plot")
        self.hour_plot_button.clicked.connect(self.show_hour_plot)
        button_layout.addWidget(self.date_plot_button)
        button_layout.addWidget(self.hour_plot_button)
        layout.addLayout(button_layout)

        self.selected_date = None
        self.selected_hour = None
        self.plot_heatmap()

        self.canvas.mpl_connect("button_press_event", self.on_heatmap_click)

    def plot_heatmap(self):
        self.ax.clear()
        sns.heatmap(self.pivot_table, cmap='YlGnBu', annot=True, fmt='.0f', ax=self.ax,
                    cbar_kws={'label': 'Production Count'})
        self.ax.set_title("Production Counts by Date and Hour")
        self.canvas.draw()

    def on_heatmap_click(self, event):
        if event.inaxes != self.ax:
            return
        col = int(event.xdata) if event.xdata is not None else None
        row = int(event.ydata) if event.ydata is not None else None
        if col is not None and row is not None and row < len(self.pivot_table.index) and col < len(self.pivot_table.columns):
            self.selected_date = self.pivot_table.index[row]
            self.selected_hour = self.pivot_table.columns[col]
            self.date_plot_button.setEnabled(True)
            self.hour_plot_button.setEnabled(True)
        else:
            self.selected_date = None
            self.selected_hour = None
            self.date_plot_button.setEnabled(False)
            self.hour_plot_button.setEnabled(False)

    def show_date_plot(self):
        if not self.selected_date:
            return
        date_records = [(t, c, p, s) for t, c, p, s in self.records if t.astimezone(IRAN_TZ).strftime('%Y-%m-%d') == self.selected_date]
        if not date_records:
            QMessageBox.information(self, "No Data", f"No records found for {self.selected_date}.")
            return
        times = [t.astimezone(IRAN_TZ) for t, _, _, _ in date_records]
        counts = [c for _, c, _, _ in date_records]
        plt.figure(figsize=(10, 6))
        plt.plot(times, counts, marker='o', linestyle='-', color='#2a9d8f')
        plt.title(f"Production Counts on {self.selected_date}")
        plt.xlabel("Time (Iran TZ, UTC+3:30)")
        plt.ylabel("Count")
        plt.grid(True)
        plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S', tz=IRAN_TZ))
        plt.gca().xaxis.set_major_locator(mdates.AutoDateLocator())
        plt.xticks(rotation=45)
        plt.tight_layout()
        plt.show()

    def show_hour_plot(self):
        if not self.selected_hour:
            return
        hour_records = [(t, c, p, s) for t, c, p, s in self.records if t.astimezone(IRAN_TZ).hour == self.selected_hour]
        if not hour_records:
            QMessageBox.information(self, "No Data", f"No records found for hour {self.selected_hour}:00.")
            return
        times = [t.astimezone(IRAN_TZ) for t, _, _, _ in hour_records]
        counts = [c for _, c, _, _ in hour_records]
        plt.figure(figsize=(10, 6))
        plt.plot(times, counts, marker='o', linestyle='-', color='#2a9d8f')
        plt.title(f"Production Counts at Hour {self.selected_hour}:00")
        plt.xlabel("Time (Iran TZ, UTC+3:30)")
        plt.ylabel("Count")
        plt.grid(True)
        plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d %H:%M:%S', tz=IRAN_TZ))
        plt.gca().xaxis.set_major_locator(mdates.AutoDateLocator())
        plt.xticks(rotation=45)
        plt.tight_layout()
        plt.show()

class PlotCanvas(FigureCanvas):
    """Matplotlib canvas for displaying interval, pulse duration, and send duration plots."""
    def __init__(self, parent=None, width=8, height=6, dpi=100, records=None, main_window=None):
        self.fig = Figure(figsize=(width, height), dpi=dpi, facecolor='#f5f6f5')
        super().__init__(self.fig)
        self.setParent(parent)
        self.ax1 = self.fig.add_subplot(311)  # Intervals
        self.ax2 = self.fig.add_subplot(312)  # Pulse durations
        self.ax3 = self.fig.add_subplot(313)  # Send durations
        self.annotation = None
        self.record_times = []
        self.intervals = []
        self.pulse_durations = []
        self.send_durations = []
        self.records = records or []
        self.main_window = main_window
        self.threshold_percent = 10
        self.exclude_anomalies = False

    def plot(self, record_times, intervals, pulse_durations, send_durations, records, anomalies=None):
        self.ax1.clear()
        self.ax2.clear()
        self.ax3.clear()
        self.record_times = record_times
        self.intervals = intervals
        self.pulse_durations = pulse_durations
        self.send_durations = send_durations
        self.records = records
        self.anomalies = anomalies if anomalies is not None else []

        if not intervals:
            self.ax1.set_title("No Intervals to Plot", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
            self.ax2.set_title("No Pulse Durations to Plot", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
            self.ax3.set_title("No Send Durations to Plot", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
            self.draw()
            return

        # Interval Plot (ax1)
        avg_interval = np.mean(intervals)
        threshold_factor = self.threshold_percent / 100
        upper_threshold = avg_interval * (1 + threshold_factor)
        lower_threshold = avg_interval * (1 - threshold_factor)

        for i in range(len(intervals)):
            color = 'red' if intervals[i] > upper_threshold else 'green' if intervals[i] < lower_threshold else 'blue'
            self.ax1.plot(record_times[i], intervals[i], marker='o', linestyle='', color=color,
                         markersize=8, markeredgecolor='#1a3c34', markeredgewidth=1)
        self.ax1.plot(record_times, intervals, linestyle='-', color='#2a9d8f', linewidth=2.5, alpha=0.5)

        self.ax1.axhline(y=upper_threshold, color='red', linestyle='--', alpha=0.5,
                        label=f'Upper Threshold ({self.threshold_percent}% above avg: {upper_threshold:.2f}s)')
        self.ax1.axhline(y=lower_threshold, color='green', linestyle='--', alpha=0.5,
                        label=f'Lower Threshold ({self.threshold_percent}% below avg: {lower_threshold:.2f}s)')
        self.ax1.axhline(y=avg_interval, color='blue', linestyle='--', alpha=0.5,
                        label=f'Average ({avg_interval:.2f}s)')

        # Pulse Duration Plot (ax2)
        if pulse_durations:
            avg_pulse = np.mean(pulse_durations)
            for i in range(len(pulse_durations)):
                color = 'red' if i in self.anomalies else 'blue'
                self.ax2.plot(record_times[i], pulse_durations[i], marker='o', linestyle='', color=color,
                             markersize=8, markeredgecolor='#1a3c34', markeredgewidth=1)
            self.ax2.plot(record_times, pulse_durations, linestyle='-', color='#2a9d8f', linewidth=2.5, alpha=0.5)
            self.ax2.axhline(y=avg_pulse, color='blue', linestyle='--', alpha=0.5,
                            label=f'Average ({avg_pulse:.2f}ms)')

        # Send Duration Plot (ax3)
        if send_durations:
            avg_send = np.mean(send_durations)
            for i in range(len(send_durations)):
                color = 'red' if i in self.anomalies else 'blue'
                self.ax3.plot(record_times[i], send_durations[i], marker='o', linestyle='', color=color,
                             markersize=8, markeredgecolor='#1a3c34', markeredgewidth=1)
            self.ax3.plot(record_times, send_durations, linestyle='-', color='#2a9d8f', linewidth=2.5, alpha=0.5)
            self.ax3.axhline(y=avg_send, color='blue', linestyle='--', alpha=0.5,
                            label=f'Average ({avg_send:.2f}ms)')

        self.ax1.set_title("Time Intervals Between Consecutive Records", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
        self.ax1.set_xlabel("Record Time (Iran TZ, UTC+3:30)", fontsize=12, color='#1a3c34')
        self.ax1.set_ylabel("Interval (seconds)", fontsize=12, color='#1a3c34')
        self.ax1.grid(True, linestyle='--', alpha=0.7, color='#d9d9d9')
        self.ax1.set_facecolor('#ffffff')
        self.ax1.legend()

        self.ax2.set_title("Pulse Durations", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
        self.ax2.set_xlabel("Record Time (Iran TZ, UTC+3:30)", fontsize=12, color='#1a3c34')
        self.ax2.set_ylabel("Duration (ms)", fontsize=12, color='#1a3c34')
        self.ax2.grid(True, linestyle='--', alpha=0.7, color='#d9d9d9')
        self.ax2.set_facecolor('#ffffff')
        self.ax2.legend()

        self.ax3.set_title("Send Durations", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
        self.ax3.set_xlabel("Record Time (Iran TZ, UTC+3:30)", fontsize=12, color='#1a3c34')
        self.ax3.set_ylabel("Duration (ms)", fontsize=12, color='#1a3c34')
        self.ax3.grid(True, linestyle='--', alpha=0.7, color='#d9d9d9')
        self.ax3.set_facecolor('#ffffff')
        self.ax3.legend()

        for ax in [self.ax1, self.ax2, self.ax3]:
            ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d %H:%M:%S', tz=IRAN_TZ))
            ax.xaxis.set_major_locator(mdates.AutoDateLocator())
            ax.tick_params(axis='x', rotation=45, labelright=True, labelsize=10, colors='#1a3c34')
            ax.tick_params(axis='y', labelsize=10, colors='#1a3c34')
            ax.spines['top'].set_visible(False)
            ax.spines['right'].set_visible(False)
            ax.spines['left'].set_color('#1a3c34')
            ax.spines['bottom'].set_color('#1a3c34')

        self.fig.tight_layout()

        self.annotation = self.ax1.annotate("", xy=(0, 0), xytext=(10, 10), textcoords="offset points",
                                           bbox=dict(boxstyle="round,pad=0.5", fc="#e9ecef", ec="#1a3c34"),
                                           arrowprops=dict(arrowstyle="->"), fontsize=10, color='#1a3c34')
        self.annotation.set_visible(False)

        self.fig.canvas.mpl_connect("motion_notify_event", self.on_hover)
        self.fig.canvas.mpl_connect("button_press_event", self.on_click)

        self.draw()

    def on_hover(self, event):
        if event.inaxes != self.ax1 or not self.record_times:
            self.annotation.set_visible(False)
            self.draw()
            return

        xdata = mdates.date2num(self.record_times)
        ydata = np.array(self.intervals)
        if event.xdata is None or event.ydata is None:
            self.annotation.set_visible(False)
            self.draw()
            return

        x_range = max(xdata) - min(xdata) if len(xdata) > 1 else 1
        y_range = max(ydata, default=1) - min(ydata, default=0) if len(ydata) > 1 else 1
        distances = np.sqrt(((xdata - event.xdata) / x_range) ** 2 + ((ydata - event.ydata) / y_range) ** 2)
        closest_idx = np.argmin(distances)

        if distances[closest_idx] < 0.05:
            self.annotation.set_visible(True)
            self.annotation.xy = (xdata[closest_idx], ydata[closest_idx])
            self.annotation.set_text(
                f"Time: {self.record_times[closest_idx].strftime('%Y-%m-%d %H:%M:%S')} (UTC+3:30)\n"
                f"Interval: {self.intervals[closest_idx]:.2f} seconds\n"
                f"Pulse Duration: {self.pulse_durations[closest_idx]:.2f} ms\n"
                f"Send Duration: {self.send_durations[closest_idx]:.2f} ms"
            )
        else:
            self.annotation.set_visible(False)

        self.draw()

    def on_click(self, event):
        if event.inaxes != self.ax1 or not self.record_times or event.button != 1:
            return

        xdata = mdates.date2num(self.record_times)
        ydata = np.array(self.intervals)
        if event.xdata is None or event.ydata is None:
            return

        x_range = max(xdata) - min(xdata) if len(xdata) > 1 else 1
        y_range = max(ydata, default=1) - min(ydata, default=0) if len(ydata) > 1 else 1
        distances = np.sqrt(((xdata - event.xdata) / x_range) ** 2 + ((ydata - event.ydata) / y_range) ** 2)
        closest_idx = np.argmin(distances)

        if distances[closest_idx] < 0.05:
            dialog = RecordsDialog(self.records, closest_idx, self.main_window)
            dialog.exec()

class PlotWindow(QMainWindow):
    """Window to display production data plots."""
    def __init__(self, record_times, intervals, pulse_durations, send_durations, records, anomalies, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Production Data Plots")
        self.setGeometry(150, 150, 1000, 800)
        self.record_times = record_times
        self.intervals = intervals
        self.pulse_durations = pulse_durations
        self.send_durations = send_durations
        self.records = records
        self.anomalies = anomalies

        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.layout = QVBoxLayout(self.central_widget)

        self.canvas = PlotCanvas(self.central_widget, records=self.records, main_window=parent)
        self.layout.addWidget(self.canvas)

        self.settings_button = QPushButton("Plot Settings")
        self.settings_button.setToolTip("Adjust the outlier threshold and anomaly settings")
        self.settings_button.clicked.connect(self.show_plot_settings)
        self.layout.addWidget(self.settings_button)

        self.layout.setStretchFactor(self.canvas, 10)
        self.layout.setStretchFactor(self.settings_button, 1)

        self.canvas.plot(self.record_times, self.intervals, self.pulse_durations, self.send_durations, self.records, self.anomalies)

    def show_plot_settings(self):
        dialog = PlotSettingsDialog(self.canvas.threshold_percent, self.canvas.exclude_anomalies, self)
        dialog.accepted.connect(self.apply_plot_settings)
        dialog.exec()

    def apply_plot_settings(self):
        dialog = PlotSettingsDialog(self.canvas.threshold_percent, self.canvas.exclude_anomalies, self)
        threshold, exclude_anomalies = dialog.get_settings()
        if threshold is not None:
            self.canvas.threshold_percent = threshold
            self.canvas.exclude_anomalies = exclude_anomalies
            self.canvas.plot(self.record_times, self.intervals, self.pulse_durations, self.send_durations, self.records, self.anomalies)

class InfluxDBApp(QMainWindow):
    """Main application window for analyzing InfluxDB production data."""
    def __init__(self):
        super().__init__()
        self.setWindowTitle("InfluxDB Production Data Analyzer")
        self.setGeometry(100, 100, 900, 700)
        self.setMinimumSize(600, 400)

        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.main_layout = QVBoxLayout(self.central_widget)
        self.main_layout.setContentsMargins(15, 15, 15, 15)
        self.main_layout.setSpacing(12)

        self.setStyleSheet("""
            QMainWindow, QWidget { background-color: #f0f4f8; }
            QGroupBox {
                font: bold 13px Helvetica;
                border: 1px solid #d0d4d8;
                border-radius: 6px;
                margin-top: 12px;
                padding: 12px;
                background-color: #ffffff;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 6px;
                color: #1a3c34;
            }
            QLabel {
                font: 11px Helvetica;
                color: #1a3c34;
                background-color: transparent;
            }
            QLineEdit, QDateEdit {
                padding: 6px;
                border: 1px solid #b0b4b8;
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
                selection-background-color: #2a9d8f;
                selection-color: #ffffff;
            }
            QPushButton {
                background-color: #2a9d8f;
                color: #ffffff;
                padding: 8px;
                border-radius: 4px;
                font: bold 11px Helvetica;
            }
            QPushButton:hover { background-color: #21867a; }
            #summaryLabel {
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

        self.device_label = QLabel("Device ID:")
        self.device_combo = QComboBox()
        self.device_combo.currentIndexChanged.connect(self.run_query)
        query_layout.addWidget(self.device_label, 0, 0)
        query_layout.addWidget(self.device_combo, 0, 1, 1, 3)

        self.start_date_label = QLabel("Start Date:")
        self.start_date_entry = QDateEdit()
        self.start_date_entry.setDisplayFormat("yyyy-MM-dd")
        self.start_date_entry.setDate(QDate.fromString(
            (datetime.now(IRAN_TZ) - timedelta(days=7)).strftime("%Y-%m-%d"), "yyyy-MM-dd"))
        query_layout.addWidget(self.start_date_label, 1, 0)
        query_layout.addWidget(self.start_date_entry, 1, 1, 1, 3)

        self.end_date_label = QLabel("End Date:")
        self.end_date_entry = QDateEdit()
        self.end_date_entry.setDisplayFormat("yyyy-MM-dd")
        self.end_date_entry.setDate(QDate.fromString(
            datetime.now(IRAN_TZ).strftime("%Y-%m-%d"), "yyyy-MM-dd"))
        query_layout.addWidget(self.end_date_label, 2, 0)
        query_layout.addWidget(self.end_date_entry, 2, 1, 1, 3)

        self.working_hours_label = QLabel("Working Hours:")
        self.work_start_time = QLineEdit("08:00:00")
        self.work_start_time.setMaximumWidth(80)
        self.work_end_time = QLineEdit("17:00:00")
        self.work_end_time.setMaximumWidth(80)
        working_hours_layout = QHBoxLayout()
        working_hours_layout.addWidget(self.working_hours_label)
        working_hours_layout.addWidget(self.work_start_time)
        working_hours_layout.addWidget(QLabel("to"))
        working_hours_layout.addWidget(self.work_end_time)
        working_hours_layout.addStretch()
        query_layout.addLayout(working_hours_layout, 3, 0, 1, 4)

        buttons_layout = QHBoxLayout()
        self.run_button = QPushButton("Run Query")
        self.run_button.clicked.connect(self.run_query)
        self.show_plot_button = QPushButton("Show Plot")
        self.show_plot_button.clicked.connect(self.show_plot_window)
        self.show_plot_button.setEnabled(False)
        self.heatmap_button = QPushButton("Show Heatmap")
        self.heatmap_button.clicked.connect(self.show_production_heatmap)
        self.heatmap_button.setEnabled(False)
        self.holiday_button = QPushButton("Set Holidays")
        self.holiday_button.clicked.connect(self.set_holidays)
        buttons_layout.addWidget(self.run_button)
        buttons_layout.addWidget(self.show_plot_button)
        buttons_layout.addWidget(self.heatmap_button)
        buttons_layout.addWidget(self.holiday_button)
        query_layout.addLayout(buttons_layout, 4, 0, 1, 4)

        self.main_layout.addWidget(query_group)

        # Results Group
        results_group = QGroupBox("Query Results")
        results_layout = QVBoxLayout()
        results_layout.setSpacing(10)
        results_group.setLayout(results_layout)

        self.summary_label = QLabel("")
        self.summary_label.setObjectName("summaryLabel")
        self.summary_label.setWordWrap(True)
        self.summary_label.setFont(QFont("Helvetica", 12))
        results_layout.addWidget(self.summary_label)

        self.rfid_button = QPushButton("Show RFID Tag Records")
        self.rfid_button.clicked.connect(self.show_rfid_records)
        self.rfid_button.setEnabled(False)
        results_layout.addWidget(self.rfid_button)

        self.main_layout.addWidget(results_group)

        self.plot_window = None
        self.heatmap_dialog = None
        self.device_info = {}
        self.records = []
        self.intervals = []
        self.record_times = []
        self.pulse_durations = []
        self.send_durations = []
        self.rfid_records = []
        self.holidays = set()
        self.exclude_weekends = True
        self.exclude_anomalies = False

        self.main_layout.setStretchFactor(query_group, 0)
        self.main_layout.setStretchFactor(results_group, 1)

        # Real-Time Monitoring
        self.timer = QTimer()
        self.timer.timeout.connect(self.run_query)
        self.timer.start(60000)  # Update every minute

        self.populate_device_ids()

    def populate_device_ids(self):
        """Queries InfluxDB for unique device IDs."""
        flux_query_devices = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
          |> range(start: -30d)
          |> filter(fn: (r) => r["_measurement"] == "{MEASUREMENT}")
          |> filter(fn: (r) => r["_field"] == "{FIELD_NAME}")
          |> group(columns: ["device"])
          |> distinct(column: "device")
        '''

        client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
        query_api = client.query_api()

        try:
            result = query_api.query(org=INFLUXDB_ORG, query=flux_query_devices)
            device_ids = []
            for table in result:
                for record in table.records:
                    device_id = record.values.get("device")
                    if device_id:
                        device_ids.append(device_id)

            self.device_info = {}
            device_ids.sort()
            self.device_combo.clear()
            if device_ids:
                self.device_combo.addItems(device_ids)
                self.device_combo.setCurrentText(device_ids[0])
                self.device_combo.setEnabled(True)
            else:
                self.device_combo.addItem("No devices found")
                self.device_combo.setEnabled(False)
                QMessageBox.warning(self, "No Devices", "No device IDs found in the database for the last 30 days.")

        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to fetch device info: {e}")
            self.device_combo.clear()
            self.device_combo.addItem("Error fetching devices")
            self.device_combo.setEnabled(False)

        finally:
            client.close()

    def set_holidays(self):
        """Opens dialog to set holidays and weekends."""
        dialog = HolidaySettingsDialog(self.holidays, self)
        if dialog.exec():
            self.holidays, self.exclude_weekends = dialog.get_holidays()
            self.run_query()

    def show_rfid_records(self):
        """Queries and displays RFID tag records."""
        device_id = self.device_combo.currentText().strip()
        if not device_id or device_id in ["No devices found", "Error fetching devices"]:
            QMessageBox.warning(self, "No Device Selected", "Please select a valid device ID first.")
            return

        start_date = self.start_date_entry.date().toString("yyyy-MM-dd")
        end_date = self.end_date_entry.date().toString("yyyy-MM-dd")
        work_start = self.work_start_time.text().strip()
        work_end = self.work_end_time.text().strip()

        try:
            start_dt_str = f"{start_date}T{work_start}"
            start_time_iran = datetime.strptime(start_dt_str, "%Y-%m-%dT%H:%M:%S").replace(tzinfo=IRAN_TZ)
            end_dt_str = f"{end_date}T{work_end}"
            end_time_iran = datetime.strptime(end_dt_str, "%Y-%m-%dT%H:%M:%S").replace(tzinfo=IRAN_TZ)
            start_time_utc = start_time_iran.astimezone(UTC_TZ)
            end_time_utc = end_time_iran.astimezone(UTC_TZ)
        except ValueError:
            QMessageBox.critical(self, "Invalid Input", "Please enter valid dates and working hours in the format shown.")
            return

        cleaned_device_id = device_id.encode('ascii', 'ignore').decode('ascii')
        escaped_device_id = escape_flux_string(cleaned_device_id)

        flux_query = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
          |> range(start: {start_time_utc.isoformat()}, stop: {end_time_utc.isoformat()})
          |> filter(fn: (r) => r["_measurement"] == "{RFID_MEASUREMENT}")
          |> filter(fn: (r) => r["_field"] == "uid")
          |> filter(fn: (r) => r["device"] == "{escaped_device_id}")
          |> sort(columns: ["_time"])
        '''

        client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
        query_api = client.query_api()

        try:
            result = query_api.query(org=INFLUXDB_ORG, query=flux_query)
            rfid_records = []
            for table in result:
                for record in table.records:
                    uid = record.get_value()
                    if uid:
                        rfid_records.append((record.get_time(), uid))

            if not rfid_records:
                QMessageBox.information(self, "No RFID Records", "No RFID tag records found in the specified time range for this device.")
                return

            dialog = RFIDRecordsDialog(rfid_records, self)
            dialog.exec()

        except Exception as e:
            QMessageBox.critical(self, "Query Error", f"Error querying RFID records: {e}")
        finally:
            client.close()

    def show_production_heatmap(self):
        """Displays an interactive heatmap of production counts."""
        if not self.records:
            QMessageBox.information(self, "No Data", "No production data available for heatmap.")
            return

        start_date = self.start_date_entry.date().toString("yyyy-MM-dd")
        end_date = self.end_date_entry.date().toString("yyyy-MM-dd")
        df = pd.DataFrame({
            'Time': [t.astimezone(IRAN_TZ) for t, _, _, _ in self.filtered_records],
            'Count': [c for _, c, _, _ in self.filtered_records]
        })
        df['Hour'] = df['Time'].dt.hour
        df['Date'] = df['Time'].dt.strftime('%Y-%m-%d')
        pivot = df.pivot_table(values='Count', index='Date', columns='Hour', aggfunc='sum', fill_value=0)
        self.heatmap_dialog = HeatmapDialog(pivot, self.filtered_records, start_date, end_date, self)
        self.heatmap_dialog.show()

    def run_query(self):
        """Queries production and RFID data, applies working hours and holiday filters, and calculates analytics."""
        device_id = self.device_combo.currentText().strip()
        if not device_id or device_id in ["No devices found", "Error fetching devices"]:
            self.summary_label.setText("Please select a valid device ID to run a query.")
            self.records = []
            self.filtered_records = []
            self.intervals = []
            self.record_times = []
            self.pulse_durations = []
            self.send_durations = []
            self.rfid_records = []
            self.anomalies = []
            self.anomaly_records = []
            self.performance_data = {}
            self.show_plot_button.setEnabled(False)
            self.heatmap_button.setEnabled(False)
            self.rfid_button.setEnabled(False)
            if self.plot_window and self.plot_window.isVisible():
                self.plot_window.close()
                self.plot_window = None
            if self.heatmap_dialog and self.heatmap_dialog.isVisible():
                self.heatmap_dialog.close()
                self.heatmap_dialog = None
            return

        start_date = self.start_date_entry.date().toString("yyyy-MM-dd")
        end_date = self.end_date_entry.date().toString("yyyy-MM-dd")
        work_start = self.work_start_time.text().strip()
        work_end = self.work_end_time.text().strip()

        self.summary_label.setText("")
        self.records = []
        self.filtered_records = []
        self.intervals = []
        self.record_times = []
        self.pulse_durations = []
        self.send_durations = []
        self.rfid_records = []
        self.anomalies = []
        self.anomaly_records = []
        self.performance_data = {}
        self.show_plot_button.setEnabled(False)
        self.heatmap_button.setEnabled(False)
        self.rfid_button.setEnabled(False)

        if self.plot_window and self.plot_window.isVisible():
            self.plot_window.close()
            self.plot_window = None
        if self.heatmap_dialog and self.heatmap_dialog.isVisible():
            self.heatmap_dialog.close()
            self.heatmap_dialog = None

        try:
            start_dt_str = f"{start_date}T{work_start}"
            start_time_iran = datetime.strptime(start_dt_str, "%Y-%m-%dT%H:%M:%S").replace(tzinfo=IRAN_TZ)
            end_dt_str = f"{end_date}T{work_end}"
            end_time_iran = datetime.strptime(end_dt_str, "%Y-%m-%dT%H:%M:%S").replace(tzinfo=IRAN_TZ)
            work_start_time = datetime.strptime(work_start, "%H:%M:%S").time()
            work_end_time = datetime.strptime(work_end, "%H:%M:%S").time()

            if end_time_iran <= start_time_iran:
                QMessageBox.critical(self, "Invalid Time Range", "End date with working hours must be after start date with working hours.")
                return
            if work_end_time <= work_start_time:
                QMessageBox.critical(self, "Invalid Working Hours", "Work end time must be after work start time.")
                return

            start_time_utc = start_time_iran.astimezone(UTC_TZ)
            end_time_utc = end_time_iran.astimezone(UTC_TZ)

        except ValueError:
            QMessageBox.critical(self, "Invalid Input", "Please enter valid dates and working hours in the format shown.")
            return

        cleaned_device_id = device_id.encode('ascii', 'ignore').decode('ascii')
        escaped_device_id = escape_flux_string(cleaned_device_id)

        flux_query_production = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
          |> range(start: {start_time_utc.isoformat()}, stop: {end_time_utc.isoformat()})
          |> filter(fn: (r) => r["_measurement"] == "{MEASUREMENT}")
          |> filter(fn: (r) => r["_field"] == "{FIELD_NAME}" or r["_field"] == "{PULSE_DURATION_FIELD}" or r["_field"] == "{SEND_DURATION_FIELD}")
          |> filter(fn: (r) => r["device"] == "{escaped_device_id}")
          |> pivot(rowKey: ["_time", "device"], columnKey: ["_field"], valueColumn: "_value")
          |> sort(columns: ["_time"])
        '''

        flux_query_rfid = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
          |> range(start: {start_time_utc.isoformat()}, stop: {end_time_utc.isoformat()})
          |> filter(fn: (r) => r["_measurement"] == "{RFID_MEASUREMENT}")
          |> filter(fn: (r) => r["_field"] == "uid")
          |> filter(fn: (r) => r["device"] == "{escaped_device_id}")
          |> sort(columns: ["_time"])
        '''

        client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
        query_api = client.query_api()

        try:
            # Query production data
            result = query_api.query(org=INFLUXDB_ORG, query=flux_query_production)
            self.records = []
            count_sum = 0
            skipped_records = 0
            for table in result:
                for record in table.records:
                    count = record.values.get("count", 0)
                    pulse_duration = record.values.get("pulse_duration_ms")
                    send_duration = record.values.get("send_duration_ms", 0)
                    # Skip only if pulse_duration is None or invalid
                    if pulse_duration is None:
                        skipped_records += 1
                        continue
                    try:
                        count = int(count) if count is not None else 0
                        pulse_duration = float(pulse_duration)
                        send_duration = float(send_duration) if send_duration is not None else 0
                    except (ValueError, TypeError):
                        count = 0
                        pulse_duration = float(pulse_duration)  # Should be valid due to earlier check
                        send_duration = 0
                        skipped_records += 1
                    self.records.append((record.get_time(), count, pulse_duration, send_duration))
                    count_sum += count

            if skipped_records > 0:
                print(f"Warning: Skipped {skipped_records} records due to missing or invalid pulse_duration_ms values.")

            # Query RFID data
            result_rfid = query_api.query(org=INFLUXDB_ORG, query=flux_query_rfid)
            self.rfid_records = []
            for table in result_rfid:
                for record in table.records:
                    uid = record.get_value()
                    if uid:
                        self.rfid_records.append((record.get_time(), uid))

            if not self.records and not self.rfid_records:
                self.summary_label.setText("No non-zero count or RFID records found in the specified time range for this device.")
                return

            self.records.sort(key=lambda x: x[0])

            # Filter records by working hours and holidays
            self.filtered_records = []
            for record in self.records:
                time_iran = record[0].astimezone(IRAN_TZ)
                date_str = time_iran.strftime("%Y-%m-%d")
                day_name = time_iran.strftime("%A")
                time_of_day = time_iran.time()
                if (time_of_day >= work_start_time and time_of_day <= work_end_time and
                    date_str not in self.holidays and
                    (not self.exclude_weekends or day_name != "Friday")):
                    self.filtered_records.append(record)

            # Anomaly Detection
            self.anomalies = []
            self.anomaly_records = []
            if self.filtered_records and len(self.filtered_records) > 1:
                pulse_durations = [r[2] for r in self.filtered_records[1:]]
                X = np.array(pulse_durations).reshape(-1, 1)
                model = IsolationForest(contamination=0.1)
                model.fit(X)
                anomalies = model.predict(X)
                self.anomalies = np.where(anomalies == -1)[0]
                self.anomaly_records = [(self.filtered_records[i+1][0], self.filtered_records[i+1][1],
                                        self.filtered_records[i+1][2], self.filtered_records[i+1][3])
                                        for i in self.anomalies]

            # Calculate intervals and durations
            filtered_records_for_calc = [r for i, r in enumerate(self.filtered_records[1:]) if not self.exclude_anomalies or i not in self.anomalies]
            if len(filtered_records_for_calc) > 0:
                self.intervals = [(filtered_records_for_calc[i+1][0] - filtered_records_for_calc[i][0]).total_seconds()
                                  for i in range(len(filtered_records_for_calc)-1)]
                self.record_times = [r[0].astimezone(IRAN_TZ) for r in filtered_records_for_calc[1:]]
                self.pulse_durations = [r[2] for r in filtered_records_for_calc[1:]]
                self.send_durations = [r[3] for r in filtered_records_for_calc[1:]]
            else:
                self.intervals = []
                self.record_times = []
                self.pulse_durations = []
                self.send_durations = []

            # Performance Analysis
            count_sum = sum(r[1] for r in self.filtered_records)
            duration_hours = 0
            if self.filtered_records:
                df = pd.DataFrame({
                    'Time': [t.astimezone(IRAN_TZ) for t, _, _, _ in self.filtered_records],
                    'Count': [c for _, c, _, _ in self.filtered_records]
                })
                df['Hour'] = df['Time'].dt.hour
                df['Date'] = df['Time'].dt.strftime('%Y-%m-%d')
                pivot = df.pivot_table(values='Count', index='Date', columns='Hour', aggfunc='sum', fill_value=0)

                # Calculate working hours per day
                daily_hours = (datetime.combine(datetime.today(), work_end_time) -
                               datetime.combine(datetime.today(), work_start_time)).total_seconds() / 3600
                unique_dates = df['Date'].unique()
                duration_hours = len([d for d in unique_dates if d not in self.holidays and
                                     (not self.exclude_weekends or datetime.strptime(d, '%Y-%m-%d').strftime('%A') != "Friday")]) * daily_hours

                hourly_rates = df.groupby('Hour')['Count'].sum() / duration_hours
                daily_rates = df.groupby('Date')['Count'].sum() / daily_hours
                best_hour = hourly_rates.idxmax() if not hourly_rates.empty else None
                best_date = daily_rates.idxmax() if not daily_rates.empty else None
                self.performance_data = {
                    'best_hour': best_hour,
                    'best_hour_rate': hourly_rates[best_hour] if best_hour is not None else 0,
                    'best_date': best_date,
                    'best_date_rate': daily_rates[best_date] if best_date is not None else 0,
                    'pivot_table': pivot
                }

            production_rate = count_sum / duration_hours if duration_hours > 0 else 0
            peak_hour = self.performance_data.get('best_hour')
            top_uid = None
            if self.rfid_records:
                uids = [uid for _, uid in self.rfid_records]
                uid_counts = pd.Series(uids).value_counts()
                top_uid = uid_counts.idxmax() if not uid_counts.empty else None

            summary_text = (
                f"<b>📊 Total Count:</b> {count_sum}<br>"
                f"<b>🏭 Production Rate:</b> {production_rate:.2f} products/hour<br>"
                f"<b>🏷 RFID Tags Detected:</b> {len(self.rfid_records)}<br>"
            )
            if peak_hour is not None:
                summary_text += f"<b>🌟 Best Production Hour:</b> {peak_hour}:00 ({self.performance_data['best_hour_rate']:.2f} products/hour)<br>"
                summary_text += f"<b>🌟 Best Production Date:</b> {self.performance_data['best_date']} ({self.performance_data['best_date_rate']:.2f} products/hour)<br>"
            if top_uid:
                summary_text += f"<b>🏷 Most Frequent RFID Tag:</b> {top_uid} ({uid_counts[top_uid]} detections)<br>"
            if self.filtered_records:
                start_record_time_iran = self.filtered_records[0][0].astimezone(IRAN_TZ)
                end_record_time_iran = self.filtered_records[-1][0].astimezone(IRAN_TZ)
                duration = end_record_time_iran - start_record_time_iran
                summary_text += (
                    f"<b>⏱ Duration of Records:</b> {duration}<br>"
                    f"<b>🕒 First Record Time (Iran TZ, UTC+3:30):</b> {start_record_time_iran.strftime('%Y-%m-%d %H:%M:%S')}<br>"
                    f"<b>🕔 Last Record Time (Iran TZ, UTC+3:30):</b> {end_record_time_iran.strftime('%Y-%m-%d %H:%M:%S')}<br>"
                )
            if len(self.filtered_records) > 1:
                avg_interval = np.mean(self.intervals)
                interval_std = np.std(self.intervals)
                summary_text += f"<b>⏲ Average Interval:</b> {avg_interval:.2f} seconds<br>"
                summary_text += f"<b>📈 Interval Std Dev:</b> {interval_std:.2f} seconds<br>"
            if self.pulse_durations:
                avg_pulse = np.mean(self.pulse_durations)
                pulse_std = np.std(self.pulse_durations)
                summary_text += f"<b>⏲ Average Pulse Duration:</b> {avg_pulse:.2f} ms<br>"
                summary_text += f"<b>📈 Pulse Duration Std Dev:</b> {pulse_std:.2f} ms<br>"
                summary_text += f"<b>⚠ Pulse Anomalies:</b> {len(self.anomalies)}<br>"
            if self.send_durations:
                avg_send = np.mean(self.send_durations)
                send_std = np.std(self.send_durations)
                summary_text += f"<b>⏲ Average Send Duration:</b> {avg_send:.2f} ms<br>"
                summary_text += f"<b>📈 Send Duration Std Dev:</b> {send_std:.2f} ms<br>"
            if len(self.filtered_records) <= 1:
                summary_text += "<b>ℹ Note:</b> Insufficient records for interval analysis after filtering."
            if skipped_records > 0:
                summary_text += f"<b>⚠ Warning:</b> Skipped {skipped_records} records due to missing or invalid pulse_duration_ms values.<br>"

            self.summary_label.setText(summary_text)

            self.show_plot_button.setEnabled(bool(self.filtered_records))
            self.heatmap_button.setEnabled(bool(self.filtered_records))
            self.rfid_button.setEnabled(True)

        except Exception as e:
            QMessageBox.critical(self, "Query Error", f"Error querying InfluxDB: {e}")
            self.show_plot_button.setEnabled(False)
            self.heatmap_button.setEnabled(False)
            self.rfid_button.setEnabled(False)

        finally:
            client.close()

    def show_plot_window(self):
        if not self.filtered_records or not self.intervals:
            QMessageBox.information(self, "No Plot Data", "Run a query first to get data for the plot.")
            return

        self.plot_window = PlotWindow(self.record_times, self.intervals, self.pulse_durations, self.send_durations,
                                     self.filtered_records, self.anomalies, parent=self)
        self.plot_window.show()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = InfluxDBApp()
    window.show()
    sys.exit(app.exec())