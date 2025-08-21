import sys
import requests
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, 
                            QPushButton, QLineEdit, QLabel, QTextEdit, QGroupBox, QMessageBox,
                            QFileDialog)
from PyQt5.QtCore import Qt, QTimer

class DeviceControlUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Hydraulic Press Control")
        self.setGeometry(100, 100, 600, 400)
        
        # Device API base URL
        self.base_url = "http://192.168.1.86"
        
        # Initialize UI
        self.init_ui()
        
        # Timer for periodic status updates
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_status)
        self.timer.start(5000)  # Update every 5 seconds
        
        # Initial status update
        self.update_status()

    def init_ui(self):
        # Main widget and layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # Status display
        status_group = QGroupBox("Device Status")
        status_layout = QVBoxLayout()
        self.status_display = QTextEdit()
        self.status_display.setReadOnly(True)
        status_layout.addWidget(self.status_display)
        status_group.setLayout(status_layout)
        main_layout.addWidget(status_group)

        # Relay control
        relay_group = QGroupBox("Relay Control")
        relay_layout = QHBoxLayout()
        self.relay_on_btn = QPushButton("Turn Relay On")
        self.relay_off_btn = QPushButton("Turn Relay Off")
        self.relay_on_btn.clicked.connect(lambda: self.control_relay("on"))
        self.relay_off_btn.clicked.connect(lambda: self.control_relay("off"))
        relay_layout.addWidget(self.relay_on_btn)
        relay_layout.addWidget(self.relay_off_btn)
        relay_group.setLayout(relay_layout)
        main_layout.addWidget(relay_group)

        # RFID write
        rfid_group = QGroupBox("Write RFID")
        rfid_layout = QHBoxLayout()
        self.rfid_input = QLineEdit()
        self.rfid_input.setPlaceholderText("Enter tag data (e.g., personnel:12345 or die:67890)")
        self.rfid_write_btn = QPushButton("Write RFID")
        self.rfid_write_btn.clicked.connect(self.write_rfid)
        rfid_layout.addWidget(self.rfid_input)
        rfid_layout.addWidget(self.rfid_write_btn)
        rfid_group.setLayout(rfid_layout)
        main_layout.addWidget(rfid_group)

        # Firmware update
        firmware_group = QGroupBox("Firmware Update")
        firmware_layout = QHBoxLayout()
        self.firmware_path = QLineEdit()
        self.firmware_path.setPlaceholderText("Select .bin file")
        self.firmware_path.setReadOnly(True)
        self.browse_btn = QPushButton("Browse")
        self.update_btn = QPushButton("Update Firmware")
        self.browse_btn.clicked.connect(self.browse_firmware)
        self.update_btn.clicked.connect(self.update_firmware)
        firmware_layout.addWidget(self.firmware_path)
        firmware_layout.addWidget(self.browse_btn)
        firmware_layout.addWidget(self.update_btn)
        firmware_group.setLayout(firmware_layout)
        main_layout.addWidget(firmware_group)

        # Reset EEPROM
        reset_group = QGroupBox("Reset EEPROM")
        reset_layout = QHBoxLayout()
        self.reset_btn = QPushButton("Reset EEPROM")
        self.reset_btn.clicked.connect(self.reset_eeprom)
        reset_layout.addWidget(self.reset_btn)
        reset_group.setLayout(reset_layout)
        main_layout.addWidget(reset_group)

    def update_status(self):
        try:
            response = requests.get(f"{self.base_url}/status", timeout=2)
            if response.status_code == 200:
                data = response.json()
                status_text = (
                    f"Device: {data['device']}\n"
                    f"WiFi Signal: {data['wifi_signal']}\n"
                    f"IP Address: {data['ip_address']}\n"
                    f"Relay Status: {data['relay_status']}\n"
                    f"Unsent Pulses: {data['unsent_count']}\n"
                    f"Failed Pulses: {data['failed_count']}\n"
                    f"Last Pulse Duration: {data['last_pulse_duration']} ms\n"
                    f"Last Send Duration: {data['last_send_duration']} ms\n"
                    f"Max Send Duration: {data['max_send_duration']} ms\n"
                    f"Last Short Pulse Duration: {data['last_short_pulse_duration']} ms\n"
                    f"Min Pulse Duration: {data['min_pulse_duration']} ms\n"
                    f"RFID Status: {data['rfid_status']}\n"
                    f"Last UID: {data['last_uid']}\n"
                    f"Last Tag Data: {data['last_tag_data']}\n"
                    f"Current Die ID: {data['current_die_id']}\n"
                    f"Attempt Write Tag: {data['attempt_write_tag']}"
                )
                self.status_display.setText(status_text)
            else:
                self.status_display.setText(f"Error: Status code {response.status_code}")
        except requests.RequestException as e:
            self.status_display.setText(f"Error: Failed to connect to device - {str(e)}")

    def control_relay(self, action):
        try:
            response = requests.post(f"{self.base_url}/relay", data={"action": action}, timeout=2)
            if response.status_code == 200:
                QMessageBox.information(self, "Success", response.json().get("status", "Relay command executed"))
                self.update_status()
            else:
                QMessageBox.critical(self, "Error", f"Failed: Status code {response.status_code}")
        except requests.RequestException as e:
            QMessageBox.critical(self, "Error", f"Failed to connect to device: {str(e)}")

    def write_rfid(self):
        tag_data = self.rfid_input.text().strip()
        if not tag_data:
            QMessageBox.warning(self, "Warning", "Please enter tag data")
            return
        try:
            response = requests.post(f"{self.base_url}/write_rfid", data={"newTagData": tag_data}, timeout=2)
            if response.status_code == 200:
                QMessageBox.information(self, "Success", response.json().get("status", "RFID write initiated"))
                self.rfid_input.clear()
                self.update_status()
            else:
                QMessageBox.critical(self, "Error", response.json().get("error", f"Failed: Status code {response.status_code}"))
        except requests.RequestException as e:
            QMessageBox.critical(self, "Error", f"Failed to connect to device: {str(e)}")

    def reset_eeprom(self):
        reply = QMessageBox.question(self, "Confirm Reset", "Are you sure you want to reset the EEPROM?",
                                   QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes:
            try:
                response = requests.post(f"{self.base_url}/reset", timeout=2)
                if response.status_code == 200:
                    QMessageBox.information(self, "Success", response.json().get("status", "EEPROM reset initiated"))
                    self.update_status()
                else:
                    QMessageBox.critical(self, "Error", f"Failed: Status code {response.status_code}")
            except requests.RequestException as e:
                QMessageBox.critical(self, "Error", f"Failed to connect to device: {str(e)}")

    def browse_firmware(self):
        file_path, _ = QFileDialog.getOpenFileName(self, "Select Firmware File", "", "Binary Files (*.bin)")
        if file_path:
            self.firmware_path.setText(file_path)

    def update_firmware(self):
        file_path = self.firmware_path.text()
        if not file_path or not file_path.endswith(".bin"):
            QMessageBox.warning(self, "Warning", "Please select a valid .bin file")
            return
        
        reply = QMessageBox.question(self, "Confirm Update", "Are you sure you want to update the firmware? This may disrupt device operation.",
                                   QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes:
            try:
                with open(file_path, "rb") as file:
                    files = {"file": (file_path.split("/")[-1], file, "application/octet-stream")}
                    auth = ("admin", "password")
                    response = requests.post(f"{self.base_url}/update", files=files, auth=auth, timeout=10)
                    if response.status_code == 200:
                        QMessageBox.information(self, "Success", "Firmware update initiated successfully")
                        self.firmware_path.clear()
                        self.update_status()
                    else:
                        QMessageBox.critical(self, "Error", f"Failed: Status code {response.status_code}")
            except requests.RequestException as e:
                QMessageBox.critical(self, "Error", f"Failed to connect to device: {str(e)}")
            except FileNotFoundError:
                QMessageBox.critical(self, "Error", "Selected file not found")

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = DeviceControlUI()
    window.show()
    sys.exit(app.exec_())