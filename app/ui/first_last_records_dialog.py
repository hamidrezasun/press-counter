from PyQt6.QtWidgets import QDialog, QVBoxLayout, QTableWidget, QTableWidgetItem, QPushButton
from PyQt6.QtCore import Qt
from data_processing.utils import IRAN_TZ

class FirstLastRecordsDialog(QDialog):
    """Dialog to display first and last records per day."""
    def __init__(self, first_last_records, parent=None):
        super().__init__(parent)
        self.setWindowTitle("First and Last Records per Day")
        self.setGeometry(200, 200, 600, 400)
        self.first_last_records = first_last_records

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
        self.table.setRowCount(len(first_last_records) if first_last_records else 1)
        self.table.setColumnCount(5)
        self.table.setHorizontalHeaderLabels(["Date", "Type", "Timestamp", "Count", "Pulse Duration (ms)"])
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.table.setSelectionMode(QTableWidget.SelectionMode.SingleSelection)
        self.table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)

        # Populate table or show no records message
        if not first_last_records:
            no_data_item = QTableWidgetItem("No records found for the selected day")
            no_data_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.table.setItem(0, 0, no_data_item)
            self.table.setSpan(0, 0, 1, 5)
        else:
            for row, (date, record_type, timestamp, count, pulse_duration) in enumerate(first_last_records):
                date_item = QTableWidgetItem(date)
                type_item = QTableWidgetItem(record_type)
                timestamp_item = QTableWidgetItem(timestamp.astimezone(IRAN_TZ).strftime("%Y-%m-%d %H:%M:%S"))
                count_item = QTableWidgetItem(str(count))
                pulse_item = QTableWidgetItem(f"{pulse_duration:.2f}")
                for item in [date_item, type_item, timestamp_item, count_item, pulse_item]:
                    item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
                self.table.setItem(row, 0, date_item)
                self.table.setItem(row, 1, type_item)
                self.table.setItem(row, 2, timestamp_item)
                self.table.setItem(row, 3, count_item)
                self.table.setItem(row, 4, pulse_item)

        self.table.resizeColumnsToContents()
        layout.addWidget(self.table)

        # Close button
        close_button = QPushButton("Close")
        close_button.clicked.connect(self.accept)
        layout.addWidget(close_button)