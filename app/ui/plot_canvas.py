from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import matplotlib.dates as mdates
import numpy as np
from data_processing.utils import IRAN_TZ
from ui.records_dialog import RecordsDialog

class PlotCanvas(FigureCanvas):
    """Matplotlib canvas for displaying interval, pulse duration, and send duration plots."""
    def __init__(self, parent=None, width=8, height=6, dpi=100, records=None, main_window=None):
        self.fig = Figure(figsize=(width, height), dpi=dpi, facecolor='#f5f6f5')
        super().__init__(self.fig)
        self.setParent(parent)
        self.ax1 = self.fig.add_subplot(311)  # Intervals
        self.ax2 = self.fig.add_subplot(312)  # Pulse durations
        self.ax3 = self.fig.add_subplot(313)  # Send durations
        self.annotation = None
        self.record_times = []
        self.intervals = []
        self.pulse_durations = []
        self.send_durations = []
        self.records = records or []
        self.main_window = main_window
        self.threshold_percent = 10
        self.exclude_anomalies = False

    def plot(self, record_times, intervals, pulse_durations, send_durations, records, anomalies=None):
        self.ax1.clear()
        self.ax2.clear()
        self.ax3.clear()
        self.record_times = record_times
        self.intervals = intervals
        self.pulse_durations = pulse_durations
        self.send_durations = send_durations
        self.records = records
        self.anomalies = anomalies if anomalies is not None else []

        if not intervals:
            self.ax1.set_title("No Intervals to Plot", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
            self.ax2.set_title("No Pulse Durations to Plot", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
            self.ax3.set_title("No Send Durations to Plot", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
            self.draw()
            return

        # Interval Plot (ax1)
        avg_interval = np.mean(intervals)
        threshold_factor = self.threshold_percent / 100
        upper_threshold = avg_interval * (1 + threshold_factor)
        lower_threshold = avg_interval * (1 - threshold_factor)

        for i in range(len(intervals)):
            color = 'red' if intervals[i] > upper_threshold else 'green' if intervals[i] < lower_threshold else 'blue'
            self.ax1.plot(record_times[i], intervals[i], marker='o', linestyle='', color=color,
                         markersize=8, markeredgecolor='#1a3c34', markeredgewidth=1)
        self.ax1.plot(record_times, intervals, linestyle='-', color='#2a9d8f', linewidth=2.5, alpha=0.5)

        self.ax1.axhline(y=upper_threshold, color='red', linestyle='--', alpha=0.5,
                        label=f'Upper Threshold ({self.threshold_percent}% above avg: {upper_threshold:.2f}s)')
        self.ax1.axhline(y=lower_threshold, color='green', linestyle='--', alpha=0.5,
                        label=f'Lower Threshold ({self.threshold_percent}% below avg: {lower_threshold:.2f}s)')
        self.ax1.axhline(y=avg_interval, color='blue', linestyle='--', alpha=0.5,
                        label=f'Average ({avg_interval:.2f}s)')

        # Pulse Duration Plot (ax2)
        if pulse_durations:
            avg_pulse = np.mean(pulse_durations)
            for i in range(len(pulse_durations)):
                color = 'red' if i in self.anomalies else 'blue'
                self.ax2.plot(record_times[i], pulse_durations[i], marker='o', linestyle='', color=color,
                             markersize=8, markeredgecolor='#1a3c34', markeredgewidth=1)
            self.ax2.plot(record_times, pulse_durations, linestyle='-', color='#2a9d8f', linewidth=2.5, alpha=0.5)
            self.ax2.axhline(y=avg_pulse, color='blue', linestyle='--', alpha=0.5,
                            label=f'Average ({avg_pulse:.2f}ms)')

        # Send Duration Plot (ax3)
        if send_durations:
            avg_send = np.mean(send_durations)
            for i in range(len(send_durations)):
                color = 'red' if i in self.anomalies else 'blue'
                self.ax3.plot(record_times[i], send_durations[i], marker='o', linestyle='', color=color,
                             markersize=8, markeredgecolor='#1a3c34', markeredgewidth=1)
            self.ax3.plot(record_times, send_durations, linestyle='-', color='#2a9d8f', linewidth=2.5, alpha=0.5)
            self.ax3.axhline(y=avg_send, color='blue', linestyle='--', alpha=0.5,
                            label=f'Average ({avg_send:.2f}ms)')

        self.ax1.set_title("Time Intervals Between Consecutive Records", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
        self.ax1.set_xlabel("Record Time (Iran TZ, UTC+3:30)", fontsize=12, color='#1a3c34')
        self.ax1.set_ylabel("Interval (seconds)", fontsize=12, color='#1a3c34')
        self.ax1.grid(True, linestyle='--', alpha=0.7, color='#d9d9d9')
        self.ax1.set_facecolor('#ffffff')
        self.ax1.legend()

        self.ax2.set_title("Pulse Durations", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
        self.ax2.set_xlabel("Record Time (Iran TZ, UTC+3:30)", fontsize=12, color='#1a3c34')
        self.ax2.set_ylabel("Duration (ms)", fontsize=12, color='#1a3c34')
        self.ax2.grid(True, linestyle='--', alpha=0.7, color='#d9d9d9')
        self.ax2.set_facecolor('#ffffff')
        self.ax2.legend()

        self.ax3.set_title("Send Durations", fontsize=16, pad=20, fontweight='bold', color='#1a3c34')
        self.ax3.set_xlabel("Record Time (Iran TZ, UTC+3:30)", fontsize=12, color='#1a3c34')
        self.ax3.set_ylabel("Duration (ms)", fontsize=12, color='#1a3c34')
        self.ax3.grid(True, linestyle='--', alpha=0.7, color='#d9d9d9')
        self.ax3.set_facecolor('#ffffff')
        self.ax3.legend()

        for ax in [self.ax1, self.ax2, self.ax3]:
            ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d %H:%M:%S', tz=IRAN_TZ))
            ax.xaxis.set_major_locator(mdates.AutoDateLocator())
            ax.tick_params(axis='x', rotation=45, labelright=True, labelsize=10, colors='#1a3c34')
            ax.tick_params(axis='y', labelsize=10, colors='#1a3c34')
            ax.spines['top'].set_visible(False)
            ax.spines['right'].set_visible(False)
            ax.spines['left'].set_color('#1a3c34')
            ax.spines['bottom'].set_color('#1a3c34')

        self.fig.tight_layout()

        self.annotation = self.ax1.annotate("", xy=(0, 0), xytext=(10, 10), textcoords="offset points",
                                           bbox=dict(boxstyle="round,pad=0.5", fc="#e9ecef", ec="#1a3c34"),
                                           arrowprops=dict(arrowstyle="->"), fontsize=10, color='#1a3c34')
        self.annotation.set_visible(False)

        self.fig.canvas.mpl_connect("motion_notify_event", self.on_hover)
        self.fig.canvas.mpl_connect("button_press_event", self.on_click)

        self.draw()

    def on_hover(self, event):
        if event.inaxes != self.ax1 or not self.record_times:
            self.annotation.set_visible(False)
            self.draw()
            return

        xdata = mdates.date2num(self.record_times)
        ydata = np.array(self.intervals)
        if event.xdata is None or event.ydata is None:
            self.annotation.set_visible(False)
            self.draw()
            return

        x_range = max(xdata) - min(xdata) if len(xdata) > 1 else 1
        y_range = max(ydata, default=1) - min(ydata, default=0) if len(ydata) > 1 else 1
        distances = np.sqrt(((xdata - event.xdata) / x_range) ** 2 + ((ydata - event.ydata) / y_range) ** 2)
        closest_idx = np.argmin(distances)

        if distances[closest_idx] < 0.05:
            self.annotation.set_visible(True)
            self.annotation.xy = (xdata[closest_idx], ydata[closest_idx])
            self.annotation.set_text(
                f"Time: {self.record_times[closest_idx].strftime('%Y-%m-%d %H:%M:%S')} (UTC+3:30)\n"
                f"Interval: {self.intervals[closest_idx]:.2f} seconds\n"
                f"Pulse Duration: {self.pulse_durations[closest_idx]:.2f} ms\n"
                f"Send Duration: {self.send_durations[closest_idx]:.2f} ms"
            )
        else:
            self.annotation.set_visible(False)

        self.draw()

    def on_click(self, event):
        if event.inaxes != self.ax1 or not self.record_times or event.button != 1:
            return

        xdata = mdates.date2num(self.record_times)
        ydata = np.array(self.intervals)
        if event.xdata is None or event.ydata is None:
            return

        x_range = max(xdata) - min(xdata) if len(xdata) > 1 else 1
        y_range = max(ydata, default=1) - min(ydata, default=0) if len(ydata) > 1 else 1
        distances = np.sqrt(((xdata - event.xdata) / x_range) ** 2 + ((ydata - event.ydata) / y_range) ** 2)
        closest_idx = np.argmin(distances)

        if distances[closest_idx] < 0.05:
            dialog = RecordsDialog(self.records, closest_idx, self.main_window)
            dialog.exec()