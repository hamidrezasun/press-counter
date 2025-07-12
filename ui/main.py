import sys
from PyQt6.QtWidgets import QApplication
from ui.main_window import InfluxDBApp

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = InfluxDBApp()
    window.show()
    sys.exit(app.exec())