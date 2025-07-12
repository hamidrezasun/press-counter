from PyQt6.QtWidgets import QDialog, QVBoxLayout, QTableWidget, QTableWidgetItem, QPushButton
from PyQt6.QtCore import Qt
from data_processing.utils import IRAN_TZ

class RFIDRecordsDialog(QDialog):
    """Dialog to display RFID tag records in a table."""
    def __init__(self, rfid_records, parent=None):
        super().__init__(parent)
        self.setWindowTitle("RFID Tag Records")
        self.setGeometry(200, 200, 600, 400)
        self.rfid_records = rfid_records

        layout = QVBoxLayout()
        self.setLayout(layout)

        # Create table
        self.table = QTableWidget()
        self.table.setRowCount(len(rfid_records))
        self.table.setColumnCount(3)
        self.table.setHorizontalHeaderLabels(["Timestamp", "UID", "Type"])
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.table.setSelectionMode(QTableWidget.SelectionMode.SingleSelection)
        self.table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)

        # Populate table
        for row, (timestamp, uid, measurement) in enumerate(rfid_records):
            timestamp_item = QTableWidgetItem(timestamp.astimezone(IRAN_TZ).strftime("%Y-%m-%d %H:%M:%S"))
            uid_item = QTableWidgetItem(uid)
            type_item = QTableWidgetItem("Die" if measurement == "rfid_tag_die" else "Operator")
            timestamp_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            uid_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            type_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.table.setItem(row, 0, timestamp_item)
            self.table.setItem(row, 1, uid_item)
            self.table.setItem(row, 2, type_item)

        self.table.resizeColumnsToContents()

        layout.addWidget(self.table)

        # Close button
        close_button = QPushButton("Close")
        close_button.clicked.connect(self.accept)
        layout.addWidget(close_button)