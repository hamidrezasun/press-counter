from PyQt6.QtWidgets import QDialog, QVBoxLayout, QTableWidget, QTableWidgetItem
from PyQt6.QtCore import Qt
from data_processing.utils import IRAN_TZ

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