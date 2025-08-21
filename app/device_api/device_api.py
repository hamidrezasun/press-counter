import requests
from PyQt6.QtWidgets import QMessageBox

def get_device_status(ip):
    """Fetches device status from the ESP8266."""
    try:
        response = requests.get(f"http://{ip}/status", timeout=5)
        response.raise_for_status()
        return response.json()
    except requests.RequestException as e:
        QMessageBox.critical(None, "Error", f"Failed to fetch device status: {e}")
        return None

def toggle_relay(ip, action):
    """Toggles the relay on or off."""
    try:
        response = requests.post(f"http://{ip}/relay", data={"action": action}, timeout=5)
        response.raise_for_status()
        return True, response.json().get("status", "Success")
    except requests.RequestException as e:
        return False, f"Failed to toggle relay: {e}"

def reset_eeprom(ip):
    """Resets the device's EEPROM."""
    try:
        response = requests.post(f"http://{ip}/reset", timeout=5)
        response.raise_for_status()
        return True, response.json().get("status", "EEPROM reset successfully")
    except requests.RequestException as e:
        return False, f"Failed to reset EEPROM: {e}"

def write_rfid(ip, uid):
    """Writes a new RFID UID to the device."""
    try:
        response = requests.post(f"http://{ip}/write_rfid", data={"newTagData": uid}, timeout=5)
        response.raise_for_status()
        return True, response.json().get("status", "Successfully wrote RFID UID")
    except requests.RequestException as e:
        return False, f"Failed to write RFID: {e}"

def update_firmware(ip, file_path):
    """Uploads a firmware file to the device."""
    try:
        with open(file_path, "rb") as file:
            response = requests.post(f"http://{ip}/update", auth=("admin", "password"), files={"file": file}, timeout=30)
            response.raise_for_status()
            return True, "Firmware updated successfully"
    except requests.RequestException as e:
        return False, f"Failed to update firmware: {e}"

def set_die_id(ip, die_id):
    """Sets a new die ID for the device."""
    try:
        response = requests.post(f"http://{ip}/set_die_id", data={"dieID": die_id}, timeout=5)
        response.raise_for_status()
        return True, response.json().get("status", "Die ID set successfully")
    except requests.RequestException as e:
        return False, f"Failed to set die ID: {e}"