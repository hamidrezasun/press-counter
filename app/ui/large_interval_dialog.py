from PyQt6.QtWidgets import QDialog, QVBoxLayout, QTableWidget, QTableWidgetItem, QPushButton
from PyQt6.QtCore import Qt
from data_processing.utils import IRAN_TZ

class LargeIntervalDialog(QDialog):
    """Dialog to display records with large intervals."""
    def __init__(self, large_interval_records, intervals, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Records with Large Intervals")
        self.setGeometry(200, 200, 600, 400)
        self.large_interval_records = large_interval_records
        self.intervals = intervals

        layout = QVBoxLayout()
        self.setLayout(layout)

        self.setStyleSheet("""
            QDialog { background-color: #f0f4f8; }
            QTableWidget {
                font: 11px Helvetica;
                color: #000000;
                background-color: #ffffff;
                border: 1px solid #d0d4d8;
                border-radius: 4px;
            }
            QTableWidget::item {
                color: #000000;
            }
            QHeaderView::section {
                color: #000000;
                background-color: #e0e4e8;
                padding: 4px;
                border: 1px solid #d0d4d8;
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
        """)

        # Create table
        self.table = QTableWidget()
        self.table.setRowCount(len(large_interval_records))
        self.table.setColumnCount(6)
        self.table.setHorizontalHeaderLabels(["Timestamp", "Count", "Pulse Duration (ms)", "Send Duration (ms)", "Interval (min)", "Delayed Send"])
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.table.setSelectionMode(QTableWidget.SelectionMode.SingleSelection)
        self.table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)

        # Populate table
        for row, (record_idx, (timestamp, count, pulse_duration, send_duration)) in enumerate(large_interval_records):
            timestamp_item = QTableWidgetItem(timestamp.astimezone(IRAN_TZ).strftime("%Y-%m-%d %H:%M:%S"))
            count_item = QTableWidgetItem(str(count))
            pulse_item = QTableWidgetItem(f"{pulse_duration:.2f}")
            send_item = QTableWidgetItem(f"{send_duration:.2f}")
            interval = self.intervals[record_idx - 1] if record_idx > 0 else 0
            interval_item = QTableWidgetItem(f"{interval:.2f}")
            delayed_send = "Yes" if count > 1 else "No"
            delayed_item = QTableWidgetItem(delayed_send)
            for item in [timestamp_item, count_item, pulse_item, send_item, interval_item, delayed_item]:
                item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.table.setItem(row, 0, timestamp_item)
            self.table.setItem(row, 1, count_item)
            self.table.setItem(row, 2, pulse_item)
            self.table.setItem(row, 3, send_item)
            self.table.setItem(row, 4, interval_item)
            self.table.setItem(row, 5, delayed_item)

        self.table.resizeColumnsToContents()
        layout.addWidget(self.table)

        # Close button
        close_button = QPushButton("Close")
        close_button.clicked.connect(self.accept)
        layout.addWidget(close_button)