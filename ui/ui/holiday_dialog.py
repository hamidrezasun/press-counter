from PyQt6.QtWidgets import QDialog, QVBoxLayout, QHBoxLayout, QLabel, QCheckBox, QPushButton, QCalendarWidget
from PyQt6.QtCore import QDate, Qt
from PyQt6.QtGui import QTextCharFormat, QColor

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