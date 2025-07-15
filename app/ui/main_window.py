from PyQt6.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QGridLayout, QHBoxLayout,
                            QLabel, QComboBox, QDateEdit, QLineEdit, QPushButton, QGroupBox, QMessageBox, QTextEdit)
from PyQt6.QtGui import QFont
from PyQt6.QtCore import QDate, QTimer
from ui.plot_window import PlotWindow
from ui.heatmap_dialog import HeatmapDialog
from ui.rfid_dialog import RFIDRecordsDialog
from ui.anomaly_dialog import AnomalyDialog
from ui.device_control_dialog import DeviceControlDialog
from ui.large_interval_dialog import LargeIntervalDialog
from ui.first_last_records_dialog import FirstLastRecordsDialog
from ui.rssi_heatmap_dialog import RSSIHeatmapDialog
from influxdb_fetch.queries import populate_device_ids, query_rfid_records, query_production_data, query_device_ip
from data_processing.analytics import calculate_analytics
from data_processing.utils import escape_flux_string, IRAN_TZ
from datetime import datetime, timedelta
import pandas as pd

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
            QLineEdit, QDateEdit, QTextEdit {
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
        self.device_combo.currentIndexChanged.connect(self.on_device_changed)
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

        self.working_hours_label = QLabel("Working Hours (Default):")
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
        self.heatmap_button = QPushButton("Show Production Heatmap")
        self.heatmap_button.clicked.connect(self.show_production_heatmap)
        self.heatmap_button.setEnabled(False)
        self.rssi_heatmap_button = QPushButton("Show RSSI Heatmap")
        self.rssi_heatmap_button.clicked.connect(self.show_rssi_heatmap)
        self.rssi_heatmap_button.setEnabled(False)
        self.anomaly_button = QPushButton("Show Anomalies")
        self.anomaly_button.clicked.connect(self.show_anomalies)
        self.anomaly_button.setEnabled(False)
        self.large_interval_button = QPushButton("Show Large Intervals")
        self.large_interval_button.clicked.connect(self.show_large_intervals)
        self.large_interval_button.setEnabled(False)
        self.first_last_button = QPushButton("Show First/Last Records")
        self.first_last_button.clicked.connect(self.show_first_last_records)
        self.first_last_button.setEnabled(False)
        self.device_control_button = QPushButton("Device Control")
        self.device_control_button.clicked.connect(self.show_device_control)
        self.device_control_button.setEnabled(False)
        buttons_layout.addWidget(self.run_button)
        buttons_layout.addWidget(self.show_plot_button)
        buttons_layout.addWidget(self.heatmap_button)
        buttons_layout.addWidget(self.rssi_heatmap_button)
        buttons_layout.addWidget(self.anomaly_button)
        buttons_layout.addWidget(self.large_interval_button)
        buttons_layout.addWidget(self.first_last_button)
        buttons_layout.addWidget(self.device_control_button)
        query_layout.addLayout(buttons_layout, 4, 0, 1, 4)

        self.main_layout.addWidget(query_group)

        # Results Group
        results_group = QGroupBox("Query Results")
        results_layout = QVBoxLayout()
        results_layout.setSpacing(10)
        results_group.setLayout(results_layout)

        self.summary_label = QTextEdit("")
        self.summary_label.setObjectName("summaryLabel")
        self.summary_label.setReadOnly(True)
        self.summary_label.setMinimumHeight(200)
        self.summary_label.setFont(QFont("Helvetica", 12))
        results_layout.addWidget(self.summary_label)

        self.rfid_button = QPushButton("Show RFID Tag Records")
        self.rfid_button.clicked.connect(self.show_rfid_records)
        self.rfid_button.setEnabled(False)
        results_layout.addWidget(self.rfid_button)

        self.main_layout.addWidget(results_group)

        self.plot_window = None
        self.heatmap_dialog = None
        self.rssi_heatmap_dialog = None
        self.anomaly_dialog = None
        self.large_interval_dialog = None
        self.first_last_dialog = None
        self.device_control_dialog = None
        self.device_info = {}
        self.records = []
        self.filtered_records = []
        self.intervals = []
        self.record_times = []
        self.pulse_durations = []
        self.send_durations = []
        self.rfid_records = []
        self.anomalies = []
        self.anomaly_records = []
        self.large_interval_records = []
        self.first_last_records = []
        self.performance_data = {}
        self.device_ip = None

        self.main_layout.setStretchFactor(query_group, 0)
        self.main_layout.setStretchFactor(results_group, 1)

        # Real-Time Monitoring
        self.timer = QTimer()
        self.timer.timeout.connect(self.run_query)
        self.timer.start(60000)  # Update every minute

        self.populate_device_ids()

    def populate_device_ids(self):
        """Queries InfluxDB for unique device IDs."""
        self.device_info = populate_device_ids(self.device_combo)

    def on_device_changed(self):
        """Handles device selection change, fetches IP, and updates UI."""
        self.device_ip = None
        self.device_control_button.setEnabled(False)
        self.run_query()

    def show_device_control(self):
        """Opens the device control dialog."""
        if not self.device_ip:
            self.device_ip = query_device_ip(self.device_combo.currentText().strip())
            if not self.device_ip:
                QMessageBox.warning(self, "No IP", "No IP address available for the selected device.")
                return
        self.device_control_dialog = DeviceControlDialog(self.device_ip, self)
        self.device_control_dialog.show()

    def show_anomalies(self):
        """Displays a table of anomalous production records."""
        if not self.anomaly_records:
            QMessageBox.information(self, "No Anomalies", "No anomalous records found in the query results.")
            return
        self.anomaly_dialog = AnomalyDialog(self.anomaly_records, self)
        self.anomaly_dialog.exec()

    def show_large_intervals(self):
        """Displays a table of records with large intervals."""
        if not self.large_interval_records:
            QMessageBox.information(self, "No Large Intervals", "No records with large intervals found.")
            return
        self.large_interval_dialog = LargeIntervalDialog(self.large_interval_records, self.intervals, self)
        self.large_interval_dialog.exec()

    def show_first_last_records(self):
        """Displays a table of first and last records per day."""
        if not self.first_last_records:
            QMessageBox.information(self, "No Records", "No first or last records found for the selected period.")
            return
        self.first_last_dialog = FirstLastRecordsDialog(self.first_last_records, self)
        self.first_last_dialog.exec()

    def show_rfid_records(self):
        """Queries and displays RFID tag records."""
        device_id = self.device_combo.currentText().strip()
        if not device_id or device_id in ["No devices found", "Error fetching devices"]:
            QMessageBox.warning(self, "No Device Selected", "Please select a valid device ID first.")
            return

        start_date = self.start_date_entry.date().toString("yyyy-MM-dd")
        end_date = self.end_date_entry.date().toString("yyyy-MM-dd")

        rfid_records = query_rfid_records(device_id, start_date, end_date)
        if not rfid_records:
            QMessageBox.information(self, "No RFID Records", "No RFID tag records found in the specified time range for this device.")
            return

        dialog = RFIDRecordsDialog(rfid_records, self)
        dialog.exec()

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

    def show_rssi_heatmap(self):
        """Displays a 3-axis heatmap of WiFi RSSI values."""
        device_ids = [self.device_combo.itemText(i) for i in range(self.device_combo.count())]
        if not device_ids or device_ids[0] in ["No devices found", "Error fetching devices"]:
            QMessageBox.warning(self, "No Devices", "No valid device IDs available.")
            return
        start_date = self.start_date_entry.date().toString("yyyy-MM-dd")
        end_date = self.end_date_entry.date().toString("yyyy-MM-dd")
        self.rssi_heatmap_dialog = RSSIHeatmapDialog(device_ids, start_date, end_date, self)
        self.rssi_heatmap_dialog.show()

    def show_plot_window(self):
        """Displays the plot window with production data."""
        if not self.filtered_records or not self.intervals:
            QMessageBox.information(self, "No Plot Data", "Run a query first to get data for the plot.")
            return

        self.plot_window = PlotWindow(self.record_times, self.intervals, self.pulse_durations, self.send_durations,
                                     self.filtered_records, self.anomalies, parent=self)
        self.plot_window.show()

    def run_query(self):
        """Queries production and RFID data, applies daily working hours filter, and calculates analytics."""
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
            self.large_interval_records = []
            self.first_last_records = []
            self.performance_data = {}
            self.show_plot_button.setEnabled(False)
            self.heatmap_button.setEnabled(False)
            self.rssi_heatmap_button.setEnabled(False)
            self.anomaly_button.setEnabled(False)
            self.large_interval_button.setEnabled(False)
            self.first_last_button.setEnabled(False)
            self.device_control_button.setEnabled(False)
            if self.plot_window and self.plot_window.isVisible():
                self.plot_window.close()
                self.plot_window = None
            if self.heatmap_dialog and self.heatmap_dialog.isVisible():
                self.heatmap_dialog.close()
                self.heatmap_dialog = None
            if self.rssi_heatmap_dialog and self.rssi_heatmap_dialog.isVisible():
                self.rssi_heatmap_dialog.close()
                self.rssi_heatmap_dialog = None
            if self.anomaly_dialog and self.anomaly_dialog.isVisible():
                self.anomaly_dialog.close()
                self.anomaly_dialog = None
            if self.large_interval_dialog and self.large_interval_dialog.isVisible():
                self.large_interval_dialog.close()
                self.large_interval_dialog = None
            if self.first_last_dialog and self.first_last_dialog.isVisible():
                self.first_last_dialog.close()
                self.first_last_dialog = None
            if self.device_control_dialog and self.device_control_dialog.isVisible():
                self.device_control_dialog.close()
                self.device_control_dialog = None
            return

        start_date = self.start_date_entry.date().toString("yyyy-MM-dd")
        end_date = self.end_date_entry.date().toString("yyyy-MM-dd")
        work_start = self.work_start_time.text().strip()
        work_end = self.work_end_time.text().strip()

        try:
            start_dt = datetime.strptime(f"{start_date}T{work_start}", "%Y-%m-%dT%H:%M:%S").replace(tzinfo=IRAN_TZ)
            end_dt = datetime.strptime(f"{end_date}T{work_end}", "%Y-%m-%dT%H:%M:%S").replace(tzinfo=IRAN_TZ)
            work_start_time = datetime.strptime(work_start, "%H:%M:%S").time()
            work_end_time = datetime.strptime(work_end, "%H:%M:%S").time()

            if end_dt <= start_dt:
                QMessageBox.critical(self, "Invalid Time Range", "End date with working hours must be after start date with working hours.")
                return
            if work_end_time <= work_start_time:
                QMessageBox.critical(self, "Invalid Working Hours", "Work end time must be after work start time.")
                return

        except ValueError:
            QMessageBox.critical(self, "Invalid Input", "Please enter valid dates and working hours in the format shown.")
            return

        self.records, self.rfid_records, skipped_records = query_production_data(device_id, start_date, end_date)
        if not self.records and not self.rfid_records:
            self.summary_label.setText("No non-zero count or RFID records found in the specified time range for this device.")
            return

        self.device_ip = query_device_ip(device_id)
        self.device_control_button.setEnabled(bool(self.device_ip))

        self.records.sort(key=lambda x: x[0])
        self.filtered_records, self.intervals, self.record_times, self.pulse_durations, self.send_durations, \
        self.anomalies, self.anomaly_records, self.large_interval_records, self.first_last_records, \
        self.performance_data, summary_text = calculate_analytics(
            self.records, self.rfid_records, start_date, end_date, work_start, work_end, skipped_records
        )

        self.summary_label.setText(summary_text)
        self.show_plot_button.setEnabled(bool(self.filtered_records))
        self.heatmap_button.setEnabled(bool(self.filtered_records))
        self.rssi_heatmap_button.setEnabled(True)
        self.anomaly_button.setEnabled(bool(self.anomaly_records))
        self.large_interval_button.setEnabled(bool(self.large_interval_records))
        self.first_last_button.setEnabled(bool(self.first_last_records))
        self.rfid_button.setEnabled(bool(self.rfid_records))