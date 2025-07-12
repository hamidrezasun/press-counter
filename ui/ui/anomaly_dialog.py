from PyQt6.QtWidgets import QDialog, QVBoxLayout, QTableWidget, QTableWidgetItem, QPushButton
from PyQt6.QtCore import Qt
from data_processing.utils import IRAN_TZ

class AnomalyDialog(QDialog):
    """Dialog to display anomalous production records in a table."""
    def __init__(self, anomaly_records, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Anomalous Production Records")
        self.setGeometry(200, 200, 600, 400)
        self.anomaly_records = anomaly_records

        layout = QVBoxLayout()
        self.setLayout(layout)

        # Create table
        self.table = QTableWidget()
        self.table.setRowCount(len(anomaly_records))
        self.table.setColumnCount(3)
        self.table.setHorizontalHeaderLabels(["Timestamp", "Count", "Pulse Duration (ms)"])
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.table.setSelectionMode(QTableWidget.SelectionMode.SingleSelection)
        self.table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)

        # Populate table
        for row, (timestamp, count, pulse_duration, _) in enumerate(anomaly_records):
            timestamp_item = QTableWidgetItem(timestamp.astimezone(IRAN_TZ).strftime("%Y-%m-%d %H:%M:%S"))
            count_item = QTableWidgetItem(str(count))
            pulse_duration_item = QTableWidgetItem(f"{pulse_duration:.2f}")
            timestamp_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            count_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            pulse_duration_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.table.setItem(row, 0, timestamp_item)
            self.table.setItem(row, 1, count_item)
            self.table.setItem(row, 2, pulse_duration_item)

        self.table.resizeColumnsToContents()

        layout.addWidget(self.table)

        # Close button
        close_button = QPushButton("Close")
        close_button.clicked.connect(self.accept)
        layout.addWidget(close_button)