import sys
import requests
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, 
                             QLabel, QLineEdit, QPushButton, QComboBox, QFileDialog, 
                             QTextEdit, QMessageBox)
from PyQt5.QtCore import QTimer
import json
import base64

class ESP8266Client(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP8266 RFID Controller")
        self.setGeometry(100, 100, 600, 400)

        # Base URL of the ESP8266 (replace with actual IP or implement discovery)
        self.base_url = "http://192.168.1.47"  # Default IP; update as needed
        self.auth = ("admin", "password")  # For /update endpoint

        # Main widget and layout
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        layout = QVBoxLayout(main_widget)

        # Device IP input
        ip_layout = QHBoxLayout()
        ip_layout.addWidget(QLabel("Device IP:"))
        self.ip_input = QLineEdit(self.base_url)
        ip_layout.addWidget(self.ip_input)
        self.update_ip_button = QPushButton("Update IP")
        self.update_ip_button.clicked.connect(self.update_ip)
        ip_layout.addWidget(self.update_ip_button)
        layout.addLayout(ip_layout)

        # Status display
        self.status_display = QTextEdit()
        self.status_display.setReadOnly(True)
        layout.addWidget(QLabel("Device Status:"))
        layout.addWidget(self.status_display)

        # Write Tag Data
        tag_layout = QHBoxLayout()
        tag_layout.addWidget(QLabel("Die:"))
        self.tag_type_combo = QComboBox()
        self.tag_type_combo.addItems(["die", "personnel"])
        tag_layout.addWidget(self.tag_type_combo)
        tag_layout.addWidget(QLabel("ID (max 10 chars):"))
        self.tag_id_input = QLineEdit()
        self.tag_id_input.setPlaceholderText("e.g., 593322")
        tag_layout.addWidget(self.tag_id_input)
        self.write_tag_button = QPushButton("Write Tag Data")
        self.write_tag_button.clicked.connect(self.write_tag_data)
        tag_layout.addWidget(self.write_tag_button)
        layout.addLayout(tag_layout)

        # Control buttons
        control_layout = QHBoxLayout()
        self.relay_on_button = QPushButton("Relay On")
        self.relay_on_button.clicked.connect(lambda: self.control_relay("on"))
        control_layout.addWidget(self.relay_on_button)
        self.relay_off_button = QPushButton("Relay Off")
        self.relay_off_button.clicked.connect(lambda: self.control_relay("off"))
        control_layout.addWidget(self.relay_off_button)
        self.counting_enable_button = QPushButton("Enable Counting")
        self.counting_enable_button.clicked.connect(lambda: self.control_counting("enable"))
        control_layout.addWidget(self.counting_enable_button)
        self.counting_disable_button = QPushButton("Disable Counting")
        self.counting_disable_button.clicked.connect(lambda: self.control_counting("disable"))
        control_layout.addWidget(self.counting_disable_button)
        self.reset_button = QPushButton("Reset EEPROM")
        self.reset_button.clicked.connect(self.reset_eeprom)
        control_layout.addWidget(self.reset_button)
        layout.addLayout(control_layout)

        # Firmware update
        update_layout = QHBoxLayout()
        self.firmware_button = QPushButton("Select Firmware (.bin)")
        self.firmware_button.clicked.connect(self.select_firmware)
        update_layout.addWidget(self.firmware_button)
        self.firmware_path_label = QLabel("No file selected")
        update_layout.addWidget(self.firmware_path_label)
        layout.addLayout(update_layout)

        # Timer for periodic status updates
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_status)
        self.timer.start(5000)  # Update every 5 seconds

        # Initial status update
        self.update_status()

    def update_ip(self):
        self.base_url = self.ip_input.text().strip()
        if not self.base_url.startswith("http://"):
            self.base_url = "http://" + self.base_url
        self.update_status()

    def update_status(self):
        try:
            response = requests.get(f"{self.base_url}/status", timeout=5)
            response.raise_for_status()
            data = response.json()
            # Format JSON for display
            status_text = json.dumps(data, indent=2)
            self.status_display.setText(status_text)
        except requests.RequestException as e:
            QMessageBox.critical(self, "Error", f"Failed to fetch status: {str(e)}")

    def write_tag_data(self):
        die = self.tag_type_combo.currentText()
        id_value = self.tag_id_input.text().strip()

        # Validate inputs
        if len(die) == 0 or len(id_value) == 0:
            QMessageBox.critical(self, "Error", "Die and ID cannot be empty")
            return
        if len(die) > 30:
            QMessageBox.critical(self, "Error", "Die must be 30 characters or less")
            return
        if len(id_value) > 20:
            QMessageBox.critical(self, "Error", "ID must be 20 characters or less")
            return
        # Ensure only printable ASCII characters (32-126)
        if not all(32 <= ord(c) <= 126 for c in die + id_value):
            QMessageBox.critical(self, "Error", "Die and ID must contain only printable ASCII characters")
            return

        payload = {"die": die, "id": id_value}
        print(f"Sending payload to {self.base_url}/write_tag: {payload}")  # Debug log

        try:
            response = requests.post(
                f"{self.base_url}/write_tag",
                headers={"Content-Type": "application/json"},
                json=payload,
                timeout=5
            )
            response.raise_for_status()
            data = response.json()
            QMessageBox.information(self, "Success", data.get("status", "Tag data written successfully"))
        except requests.exceptions.HTTPError as e:
            error_message = f"HTTP error: {str(e)}"
            try:
                error_data = e.response.json()
                error_message += f"\nDetails: {error_data.get('error', 'No details provided')}"
                if 'details' in error_data:
                    error_message += f"\n{error_data['details']}"
            except ValueError:
                error_message += f"\nResponse: {e.response.text}"
            QMessageBox.critical(self, "Error", error_message)
        except requests.RequestException as e:
            QMessageBox.critical(self, "Error", f"Failed to write tag data: {str(e)}")

    def control_relay(self, action):
        try:
            response = requests.post(f"{self.base_url}/relay", data={"action": action}, timeout=5)
            response.raise_for_status()
            QMessageBox.information(self, "Success", response.json().get("status", f"Relay {action}"))
        except requests.RequestException as e:
            QMessageBox.critical(self, "Error", f"Failed to control relay: {str(e)}")

    def control_counting(self, action):
        try:
            response = requests.post(f"{self.base_url}/counting", data={"action": action}, timeout=5)
            response.raise_for_status()
            QMessageBox.information(self, "Success", response.json().get("status", f"Counting {action}d"))
        except requests.RequestException as e:
            QMessageBox.critical(self, "Error", f"Failed to control counting: {str(e)}")

    def reset_eeprom(self):
        try:
            response = requests.post(f"{self.base_url}/reset", timeout=5)
            response.raise_for_status()
            QMessageBox.information(self, "Success", response.json().get("status", "EEPROM reset successfully"))
        except requests.RequestException as e:
            QMessageBox.critical(self, "Error", f"Failed to reset EEPROM: {str(e)}")

    def select_firmware(self):
        file_path, _ = QFileDialog.getOpenFileName(self, "Select Firmware File", "", "Binary Files (*.bin)")
        if file_path:
            self.firmware_path_label.setText(file_path)
            reply = QMessageBox.question(self, "Confirm Update", 
                                        f"Upload {file_path} to {self.base_url}/update?", 
                                        QMessageBox.Yes | QMessageBox.No)
            if reply == QMessageBox.Yes:
                try:
                    with open(file_path, "rb") as f:
                        files = {"file": (file_path, f, "application/octet-stream")}
                        auth_header = "Basic " + base64.b64encode(b"admin:password").decode("utf-8")
                        response = requests.post(
                            f"{self.base_url}/update",
                            headers={"Authorization": auth_header},
                            files=files,
                            timeout=30  # Longer timeout for file upload
                        )
                        response.raise_for_status()
                        QMessageBox.information(self, "Success", "Firmware update initiated")
                except requests.RequestException as e:
                    QMessageBox.critical(self, "Error", f"Failed to upload firmware: {str(e)}")
                except FileNotFoundError:
                    QMessageBox.critical(self, "Error", "Firmware file not found")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = ESP8266Client()
    window.show()
    sys.exit(app.exec_())