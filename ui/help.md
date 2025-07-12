# InfluxDB Production Data Analyzer - Help

## Overview
The InfluxDB Production Data Analyzer is a PyQt6-based desktop application for monitoring and analyzing production data from a hydraulic press with an ESP8266 microcontroller. It queries production counts and RFID tag records (die and operator types) from an InfluxDB database, calculates analytics, detects anomalies (pulse duration and `count > 1`) and large intervals, and provides visualizations including a 3-axis WiFi RSSI heatmap. Device control is available via a RESTful API.

## Prerequisites
- **Python Environment**: Python 3.13 with:
  ```bash
  pip install PyQt6 influxdb-client matplotlib numpy pandas seaborn scikit-learn requests
  ```
- **InfluxDB Setup**: Server at `http://192.168.1.37:8086` with:
  - Organization: `tooswasher`
  - Bucket: `manufacture`
  - Valid authentication token
- **Network**: Must be on the same network as the ESP8266 (e.g., "TW factory" WiFi).
- **Directory Structure**:
  ```
  project/
  ├── main.py
  ├── ui/
  │   ├── main_window.py
  │   ├── records_dialog.py
  │   ├── rfid_dialog.py
  │   ├── plot_settings_dialog.py
  │   ├── heatmap_dialog.py
  │   ├── plot_canvas.py
  │   ├── plot_window.py
  │   ├── anomaly_dialog.py
  │   ├── device_control_dialog.py
  │   ├── large_interval_dialog.py
  │   ├── first_last_records_dialog.py
  │   ├── rssi_heatmap_dialog.py
  ├── data_processing/
  │   ├── utils.py
  │   ├── analytics.py
  ├── influxdb_fetch/
  │   ├── queries.py
  ├── device_api/
  │   ├── device_api.py
  ├── help.md
  ```

## Running the Application
1. Navigate to:
   ```bash
   cd C:\Users\hamidreza\Nextcloud\project\press counter\ui
   ```
2. Run:
   ```bash
   python main.py
   ```
3. Use the UI to select devices, set query parameters, view results, and control the device.

## How the Code Calculates Analytics
The application processes data from the `manufacture` bucket (`production`, `rfid_tag_die`, `rfid_tag_people`, `device_info` measurements).

### Production Data
- **Source**: `production` measurement with:
  - `count`: Number of pulses (products). If `count > 1`, indicates multiple products sent together due to failed send attempts.
  - `pulse_duration_ms`: Pulse train duration (ms).
  - `send_duration_ms`: Time to send data to InfluxDB.
- **Querying**: `query_production_data` fetches records, pivoting fields by timestamp.
- **Working Hours**:
  - Per day, the first record's timestamp is the start, and the last is the end.
  - Records are filtered within these daily bounds, constrained by user-specified default hours (e.g., 08:00:00–17:00:00).
- **Calculations**:
  - **Total Production Count**: Sum of `count` across records.
  - **Production Rate**: Total `count` ÷ total working hours (products/hour).
  - **Average Time per Product**: Total working seconds ÷ total `count` (e.g., 10 counts in 30 min = 180 sec/product, adjusted to ~3 sec/product based on example).
  - **Intervals**: Time differences (minutes) between records. Note: `count > 1` may skew intervals due to delayed sends.
  - **Large Intervals**: Intervals > 10 minutes, flagged with `count > 1` status.
  - **First/Last Records**: Define daily working hours.
  - **Pulse/Send Durations**: Lists for anomaly detection and visualization.

### RFID Records
- **Source**:
  - `rfid_tag_die`: Tags starting with '1' (die changes).
  - `rfid_tag_people`: Tags starting with '2' (operator login/logout).
- **Querying**: `query_rfid_records` fetches both types with UID and type.
- **Calculations**:
  - **Total Die RFID Scans**: Count of die changes.
  - **Total Operator RFID Scans**: Count of operator interactions.

### Anomaly Detection
- **Definition**: Anomalous `pulse_duration_ms` or `count > 1` (indicating failed sends).
- **Method**: IQR for pulse durations; values outside `[Q1 - 1.5*IQR, Q3 + 1.5*IQR]` are anomalous. All `count > 1` records are flagged.
- **Causes**:
  - Pulse anomalies: Sensor issues, jams, power fluctuations.
  - `count > 1`: Network issues causing data to be stored and sent later.

### Large Intervals
- **Definition**: Intervals > 10 minutes, indicating downtime or delayed sends.
- **Processing**: Flagged in `analytics.py`, with `count > 1` noted in the UI.

### First/Last Records
- **Purpose**: Show operational start/end times per day.
- **Details**: Includes date, type (First/Last), timestamp, count, pulse duration.

### WiFi RSSI Heatmap
- **Source**: `device_info` measurement with `wifi_signal` (RSSI in dBm).
- **Visualization**: 3-axis heatmap (Device ID vs. Date vs. Hour).

## User Interface Elements
### Query Parameters Group
- **Device ID (ComboBox)**:
  - Selects the ESP8266 device. Populated from `production` data (last 30 days). Enables device control if IP is found.
- **Start Date (QDateEdit)**:
  - Sets query start date (default: 7 days ago, `yyyy-MM-dd`).
- **End Date (QDateEdit)**:
  - Sets query end date (default: today, `yyyy-MM-dd`).
- **Working Hours (Default) (QLineEdit)**:
  - Sets default bounds (e.g., `08:00:00` to `17:00:00`). Overridden by daily first/last records.
- **Run Query (Button)**:
  - Fetches production/RFID data, filters by daily working hours, and displays analytics.
- **Show Plot (Button)**:
  - Opens a window with selectable plots (Production Count, Intervals, Pulse Durations, Send Durations).
- **Show Production Heatmap (Button)**:
  - Displays a 2D heatmap of production counts (Date vs. Hour).
- **Show RSSI Heatmap (Button)**:
  - Displays a 3-axis heatmap of WiFi RSSI (Device ID vs. Date vs. Hour).
- **Show Anomalies (Button)**:
  - Shows a table of records with anomalous pulse durations or `count > 1`.
- **Show Large Intervals (Button)**:
  - Shows a table of records following intervals > 10 minutes, noting if `count > 1`.
- **Show First/Last Records (Button)**:
  - Shows a table of first and last records per day.
- **Device Control (Button)**:
  - Opens the device control dialog (requires valid IP).

### Results Group
- **Summary Label (QTextEdit)**:
  - Displays detailed analytics:
    - **Total Production Count**: Sum of `count` (notes `count > 1` indicates delayed sends).
    - **Production Rate**: Products per hour.
    - **Average Time per Product**: Seconds per product (e.g., ~3 sec/product for 10 counts in 30 min).
    - **Total Die RFID Scans**: Number of `rfid_tag_die` records.
    - **Total Operator RFID Scans**: Number of `rfid_tag_people` records.
    - **Total Anomalies**: Anomalous pulse durations or `count > 1`.
    - **Records with Count > 1**: Number of delayed send records.
    - **Total Large Intervals**: Intervals > 10 minutes.
    - **Skipped Records**: Records with invalid `pulse_duration_ms`.
    - **Working Hours (Dynamic)**: First/last record times per day.
- **Show RFID Tag Records (Button)**:
  - Shows a table of RFID records (timestamp, UID, type: Die/Operator).

### Plot Window
- **Production Count (Button)**: Plots counts over time, with anomalies (`count > 1` or pulse) marked. Click a point to see record details.
- **Intervals (Button)**: Plots intervals between records (minutes).
- **Pulse Durations (Button)**: Plots `pulse_duration_ms`, with anomalies marked. Click for details.
- **Send Durations (Button)**: Plots `send_duration_ms`. Click for details.
- **Record Details Dialog** (on click):
  - Shows timestamp, count, pulse duration, send duration, interval, and note if `count > 1` (delayed send).

### Device Control Dialog
- **Fetch Device Status (Button)**: Retrieves real-time status via `GET /status`.
- **Relay On/Off (Buttons)**: Toggles relay via `POST /relay`.
- **Reset EEPROM (Button)**: Clears counts via `POST /reset`.
- **New RFID UID (QLineEdit)**: Sets 8-character hex UID (e.g., `1A2B3C4D`).
- **Write RFID (Button)**: Sends UID to device via `POST /write_rfid`.
- **Update Firmware (Button)**: Uploads `.bin` file via `POST /update`.
- **Status Label (QTextEdit)**:
  - Displays device metrics (e.g., WiFi signal, IP, relay status, counts, durations, RFID status).

## Usage Instructions
1. **Select Device**: Choose a device ID.
2. **Set Query Parameters**:
   - Start/End Date: Define the query range.
   - Working Hours: Set default bounds (overridden by daily first/last records).
3. **Run Query**: Fetch and analyze data.
4. **View Results**:
   - Summary: Check analytics, noting `count > 1` for delayed sends.
   - Show Plot: Select plot type; click points for record details (includes delayed send note).
   - Show Production Heatmap: View production heatmap.
   - Show RSSI Heatmap: View WiFi signal heatmap.
   - Show Anomalies: View anomalous records (pulse or `count > 1`).
   - Show Large Intervals: View records after long gaps, noting `count > 1`.
   - Show First/Last Records: View daily start/end records.
   - Show RFID Tag Records: View RFID table.
   - Device Control: Open dialog for API interactions.
5. **Device Control**: Use buttons to manage the device.

## Troubleshooting
- **InfluxDB Errors**: Verify server, token, and bucket settings.
- **Device API Errors**: Ensure network connectivity and correct IP.
- **No Data in Heatmaps**: Check `production` and `device_info` measurements.
- **Plot Click Issues**: Ensure `matplotlib` events are enabled.
- **Large Intervals Missing**: Adjust `LARGE_INTERVAL_THRESHOLD` in `analytics.py`.
- **High Count Not Flagged**: Verify `count > 1` detection in `analytics.py`.