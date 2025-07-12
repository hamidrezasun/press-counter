import requests
from PyQt6.QtWidgets import QMessageBox

def get_device_status(ip):
    """Fetches device status from the /status endpoint."""
    try:
        response = requests.get(f"http://{ip}/status", timeout=5)
        response.raise_for_status()
        return response.json()
    except requests.RequestException as e:
        return None

def toggle_relay(ip, action):
    """Toggles the relay via the /relay endpoint."""
    try:
        response = requests.post(f"http://{ip}/relay", data={"action": action}, timeout=5)
        response.raise_for_status()
        data = response.json()
        return True, data.get("status", "Relay action successful")
    except requests.RequestException as e:
        return False, f"Failed to toggle relay: {e}"

def reset_eeprom(ip):
    """Resets the EEPROM via the /reset endpoint."""
    try:
        response = requests.post(f"http://{ip}/reset", timeout=5)
        response.raise_for_status()
        data = response.json()
        return True, data.get("status", "EEPROM reset successful")
    except requests.RequestException as e:
        return False, f"Failed to reset EEPROM: {e}"

def write_rfid(ip, new_uid):
    """Writes a new RFID UID via the /write_rfid endpoint."""
    try:
        response = requests.post(f"http://{ip}/write_rfid", data={"new_uid": new_uid}, timeout=5)
        response.raise_for_status()
        data = response.json()
        return True, data.get("status", f"New UID {new_uid} set successfully")
    except requests.RequestException as e:
        return False, f"Failed to write RFID UID: {e}"

def update_firmware(ip, file_path):
    """Uploads a .bin file to the /update endpoint for firmware update."""
    try:
        with open(file_path, "rb") as file:
            files = {"file": (file_path, file, "application/octet-stream")}
            response = requests.post(
                f"http://{ip}/update",
                files=files,
                auth=("admin", "password"),
                timeout=10
            )
            response.raise_for_status()
            return True, "Firmware update successful"
    except requests.RequestException as e:
        return False, f"Failed to update firmware: {e}"
    except FileNotFoundError:
        return False, "Firmware file not found"
    except Exception as e:
        return False, f"Error during firmware update: {e}"