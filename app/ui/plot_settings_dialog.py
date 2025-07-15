from PyQt6.QtWidgets import QDialog, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit, QCheckBox, QPushButton, QMessageBox

class PlotSettingsDialog(QDialog):
    """Dialog for adjusting plot settings."""
    def __init__(self, current_threshold, exclude_anomalies, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Plot Settings")
        self.setFixedSize(300, 150)
        layout = QVBoxLayout(self)

        threshold_layout = QHBoxLayout()
        self.threshold_label = QLabel("Threshold (%):")
        self.threshold_entry = QLineEdit(str(current_threshold))
        self.threshold_entry.setToolTip("Percentage above/below average for highlighting outliers")
        self.threshold_entry.setMaximumWidth(60)
        threshold_layout.addWidget(self.threshold_label)
        threshold_layout.addWidget(self.threshold_entry)
        threshold_layout.addStretch()
        layout.addLayout(threshold_layout)

        self.exclude_anomalies_check = QCheckBox("Exclude Anomalous Pulse Durations from Calculations")
        self.exclude_anomalies_check.setChecked(exclude_anomalies)
        layout.addWidget(self.exclude_anomalies_check)

        apply_button = QPushButton("Apply")
        apply_button.clicked.connect(self.accept)
        layout.addWidget(apply_button)

    def get_settings(self):
        try:
            threshold = float(self.threshold_entry.text().strip())
            if threshold < 0:
                raise ValueError("Threshold must be non-negative")
            return threshold, self.exclude_anomalies_check.isChecked()
        except ValueError:
            QMessageBox.warning(self, "Invalid Input", "Please enter a valid non-negative percentage for the threshold.")
            return None, None