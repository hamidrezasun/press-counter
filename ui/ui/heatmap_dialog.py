from PyQt6.QtWidgets import QDialog, QVBoxLayout, QHBoxLayout, QPushButton, QMessageBox
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import matplotlib.dates as mdates
import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
from data_processing.utils import IRAN_TZ

class HeatmapDialog(QDialog):
    """Dialog to display interactive heatmap with clickable cells for date range."""
    def __init__(self, pivot_table, records, start_date, end_date, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Production Heatmap")
        self.setGeometry(200, 200, 1000, 600)
        self.pivot_table = pivot_table
        self.records = records
        self.start_date = start_date
        self.end_date = end_date
        layout = QVBoxLayout(self)

        self.canvas = FigureCanvas(Figure(figsize=(10, 6)))
        self.ax = self.canvas.figure.add_subplot(111)
        layout.addWidget(self.canvas)

        button_layout = QHBoxLayout()
        self.date_plot_button = QPushButton("Show Date Plot")
        self.date_plot_button.clicked.connect(self.show_date_plot)
        self.hour_plot_button = QPushButton("Show Hour Plot")
        self.hour_plot_button.clicked.connect(self.show_hour_plot)
        button_layout.addWidget(self.date_plot_button)
        button_layout.addWidget(self.hour_plot_button)
        layout.addLayout(button_layout)

        self.selected_date = None
        self.selected_hour = None
        self.plot_heatmap()

        self.canvas.mpl_connect("button_press_event", self.on_heatmap_click)

    def plot_heatmap(self):
        self.ax.clear()
        sns.heatmap(self.pivot_table, cmap='YlGnBu', annot=True, fmt='.0f', ax=self.ax,
                    cbar_kws={'label': 'Production Count'})
        self.ax.set_title("Production Counts by Date and Hour")
        self.canvas.draw()

    def on_heatmap_click(self, event):
        if event.inaxes != self.ax:
            return
        col = int(event.xdata) if event.xdata is not None else None
        row = int(event.ydata) if event.ydata is not None else None
        if col is not None and row is not None and row < len(self.pivot_table.index) and col < len(self.pivot_table.columns):
            self.selected_date = self.pivot_table.index[row]
            self.selected_hour = self.pivot_table.columns[col]
            self.date_plot_button.setEnabled(True)
            self.hour_plot_button.setEnabled(True)
        else:
            self.selected_date = None
            self.selected_hour = None
            self.date_plot_button.setEnabled(False)
            self.hour_plot_button.setEnabled(False)

    def show_date_plot(self):
        if not self.selected_date:
            return
        date_records = [(t, c, p, s) for t, c, p, s in self.records if t.astimezone(IRAN_TZ).strftime('%Y-%m-%d') == self.selected_date]
        if not date_records:
            QMessageBox.information(self, "No Data", f"No records found for {self.selected_date}.")
            return
        times = [t.astimezone(IRAN_TZ) for t, _, _, _ in date_records]
        counts = [c for _, c, _, _ in date_records]
        plt.figure(figsize=(10, 6))
        plt.plot(times, counts, marker='o', linestyle='-', color='#2a9d8f')
        plt.title(f"Production Counts on {self.selected_date}")
        plt.xlabel("Time (Iran TZ, UTC+3:30)")
        plt.ylabel("Count")
        plt.grid(True)
        plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S', tz=IRAN_TZ))
        plt.gca().xaxis.set_major_locator(mdates.AutoDateLocator())
        plt.xticks(rotation=45)
        plt.tight_layout()
        plt.show()

    def show_hour_plot(self):
        if not self.selected_hour:
            return
        hour_records = [(t, c, p, s) for t, c, p, s in self.records if t.astimezone(IRAN_TZ).hour == self.selected_hour]
        if not hour_records:
            QMessageBox.information(self, "No Data", f"No records found for hour {self.selected_hour}:00.")
            return
        times = [t.astimezone(IRAN_TZ) for t, _, _, _ in hour_records]
        counts = [c for _, c, _, _ in hour_records]
        plt.figure(figsize=(10, 6))
        plt.plot(times, counts, marker='o', linestyle='-', color='#2a9d8f')
        plt.title(f"Production Counts at Hour {self.selected_hour}:00")
        plt.xlabel("Time (Iran TZ, UTC+3:30)")
        plt.ylabel("Count")
        plt.grid(True)
        plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d %H:%M:%S', tz=IRAN_TZ))
        plt.gca().xaxis.set_major_locator(mdates.AutoDateLocator())
        plt.xticks(rotation=45)
        plt.tight_layout()
        plt.show()