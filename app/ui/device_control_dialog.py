from PyQt6.QtWidgets import QDialog, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit, QPushButton, QTextEdit, QFileDialog
from PyQt6.QtCore import Qt
from device_api.device_api import get_device_status, toggle_relay, reset_eeprom, write_rfid, update_firmware
from PyQt6.QtWidgets import QMessageBox

class DeviceControlDialog(QDialog):
    """Dialog for controlling device via API."""
    def __init__(self, device_ip, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Device Control")
        self.setGeometry(200, 200, 400, 500)
        self.device_ip = device_ip

        layout = QVBoxLayout()
        self.setLayout(layout)

        self.setStyleSheet("""
            QDialog { background-color: #f0f4f8; }
            QLabel { font: 11px Helvetica; color: #1a3c34; }
            QLineEdit, QTextEdit {
                padding: 6px;
                border: 1px solid #b0b4b8;
                border-radius: 4px;
                background-color: #ffffff;
                color: #1a3c34;
            }
            QPushButton {
                background-color: #2a9d8f;
                color: #ffffff;
                padding: 8px;
                border-radius: 4px;
                font: bold 11px Helvetica;
            }
            QPushButton:hover { background-color: #21867a; }
            QTextEdit#statusLabel {
                background-color: #ffffff;
                border: 1px solid #d0d4d8;
                border-radius: 6px;
                padding: 12px;
                font: 12px Helvetica;
                color: #1a3c34;
            }
        """)

        self.status_button = QPushButton("Fetch Device Status")
        self.status_button.clicked.connect(self.fetch_device_status)
        layout.addWidget(self.status_button)

        relay_layout = QHBoxLayout()
        self.relay_on_button = QPushButton("Relay On")
        self.relay_on_button.clicked.connect(lambda: self.control_relay("on"))
        self.relay_off_button = QPushButton("Relay Off")
        self.relay_off_button.clicked.connect(lambda: self.control_relay("off"))
        relay_layout.addWidget(self.relay_on_button)
        relay_layout.addWidget(self.relay_off_button)
        layout.addLayout(relay_layout)

        self.reset_button = QPushButton("Reset EEPROM")
        self.reset_button.clicked.connect(self.reset_device_eeprom)
        layout.addWidget(self.reset_button)

        rfid_layout = QHBoxLayout()
        self.rfid_uid_label = QLabel("New RFID UID:")
        self.rfid_uid_entry = QLineEdit()
        self.rfid_uid_entry.setPlaceholderText("e.g., 1A2B3C4D")
        self.rfid_uid_entry.setMaximumWidth(100)
        self.write_rfid_button = QPushButton("Write RFID")
        self.write_rfid_button.clicked.connect(self.write_rfid_uid)
        rfid_layout.addWidget(self.rfid_uid_label)
        rfid_layout.addWidget(self.rfid_uid_entry)
        rfid_layout.addWidget(self.write_rfid_button)
        layout.addLayout(rfid_layout)

        self.firmware_button = QPushButton("Update Firmware")
        self.firmware_button.clicked.connect(self.update_firmware)
        layout.addWidget(self.firmware_button)

        self.status_label = QTextEdit("")
        self.status_label.setObjectName("statusLabel")
        self.status_label.setReadOnly(True)
        self.status_label.setMinimumHeight(200)
        layout.addWidget(self.status_label)

        close_button = QPushButton("Close")
        close_button.clicked.connect(self.accept)
        layout.addWidget(close_button)

        self.fetch_device_status()

    def fetch_device_status(self):
        """Fetches and displays device status via API."""
        status = get_device_status(self.device_ip)
        if status:
            status_text = (
                f"<b>Device:</b> {status.get('device', 'N/A')}<br>"
                f"<b>WiFi Signal:</b> {status.get('wifi_signal', 'N/A')}<br>"
                f"<b>IP Address:</b> {status.get('ip_address', 'N/A')}<br>"
                f"<b>Relay Status:</b> {status.get('relay_status', 'N/A')}<br>"
                f"<b>Unsent Count:</b> {status.get('unsent_count', 'N/A')}<br>"
                f"<b>Failed Count:</b> {status.get('failed_count', 'N/A')}<br>"
                f"<b>Last Pulse Duration:</b> {status.get('last_pulse_duration', 'N/A')} ms<br>"
                f"<b>Last Send Duration:</b> {status.get('last_send_duration', 'N/A')} ms<br>"
                f"<b>Max Send Duration:</b> {status.get('max_send_duration', 'N/A')} ms<br>"
                f"<b>Last Short Pulse Duration:</b> {status.get('last_short_pulse_duration', 'N/A')} ms<br>"
                f"<b>Last Short Pulse Time:</b> {status.get('last_short_pulse_time', 'N/A')}<br>"
                f"<b>Min Pulse Duration:</b> {status.get('min_pulse_duration', 'N/A')} ms<br>"
                f"<b>RFID Status:</b> {status.get('rfid_status', 'N/A')}<br>"
                f"<b>Last UID:</b> {status.get('last_uid', 'N/A')}<br>"
                f"<b>New UID:</b> {status.get('new_uid', 'N/A')}"
            )
            self.status_label.setText(status_text)
        else:
            self.status_label.setText("Failed to fetch device status.")

    def control_relay(self, action):
        """Controls the relay via API."""
        success, message = toggle_relay(self.device_ip, action)
        if success:
            QMessageBox.information(self, "Success", message)
            self.fetch_device_status()
        else:
            QMessageBox.critical(self, "Error", message)

    def reset_device_eeprom(self):
        """Resets the device's EEPROM via API."""
        success, message = reset_eeprom(self.device_ip)
        if success:
            QMessageBox.information(self, "Success", message)
            self.fetch_device_status()
        else:
            QMessageBox.critical(self, "Error", message)

    def write_rfid_uid(self):
        """Writes a new RFID UID via API."""
        new_uid = self.rfid_uid_entry.text().strip().upper()
        if not new_uid or len(new_uid) != 8 or not all(c in '0123456789ABCDEF' for c in new_uid):
            QMessageBox.warning(self, "Invalid UID", "UID must be an 8-character hexadecimal value (e.g., 1A2B3C4D).")
            return
        success, message = write_rfid(self.device_ip, new_uid)
        if success:
            QMessageBox.information(self, "Success", message)
            self.rfid_uid_entry.clear()
            self.fetch_device_status()
        else:
            QMessageBox.critical(self, "Error", message)

    def update_firmware(self):
        """Opens a file dialog to select a .bin file and uploads it to the device."""
        file_path, _ = QFileDialog.getOpenFileName(
            self, "Select Firmware File", "", "Binary Files (*.bin)"
        )
        if not file_path:
            return
        success, message = update_firmware(self.device_ip, file_path)
        if success:
            QMessageBox.information(self, "Success", message)
            self.fetch_device_status()
        else:
            QMessageBox.critical(self, "Error", message)