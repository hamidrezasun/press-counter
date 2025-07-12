import numpy as np
from datetime import datetime, timedelta
from data_processing.utils import IRAN_TZ

LARGE_INTERVAL_THRESHOLD = 10.0  # Minutes

def calculate_analytics(records, rfid_records, start_date, end_date, work_start, work_end, skipped_records):
    """Calculates analytics for production data, using first and last records per day as working hours."""
    filtered_records = []
    intervals = []
    record_times = []
    pulse_durations = []
    send_durations = []
    anomalies = []
    anomaly_records = []
    large_interval_records = []
    first_last_records = []
    performance_data = {}
    high_count_records = []  # Track records with count > 1

    # Convert date strings to datetime
    start_dt = datetime.strptime(start_date, "%Y-%m-%d").replace(tzinfo=IRAN_TZ)
    end_dt = datetime.strptime(end_date, "%Y-%m-%d").replace(tzinfo=IRAN_TZ)

    # Group records by day
    records_by_day = {}
    for record in records:
        timestamp, count, pulse_duration, send_duration = record
        local_time = timestamp.astimezone(IRAN_TZ)
        day_key = local_time.date()
        if day_key not in records_by_day:
            records_by_day[day_key] = []
        records_by_day[day_key].append(record)
        if count > 1:
            high_count_records.append((len(filtered_records), record))  # Track index and record

    # Filter records by daily working hours and collect first/last records
    for day, day_records in records_by_day.items():
        if not day_records:
            continue
        day_records.sort(key=lambda x: x[0])
        first_record = day_records[0]
        last_record = day_records[-1]
        start_time = first_record[0].astimezone(IRAN_TZ).time()
        end_time = last_record[0].astimezone(IRAN_TZ).time()
        first_last_records.append((day.strftime("%Y-%m-%d"), "First", first_record[0], first_record[1], first_record[2]))
        first_last_records.append((day.strftime("%Y-%m-%d"), "Last", last_record[0], last_record[1], last_record[2]))
        for record in day_records:
            timestamp, count, pulse_duration, send_duration = record
            local_time = timestamp.astimezone(IRAN_TZ)
            local_time_of_day = local_time.time()
            if start_dt.date() <= local_time.date() <= end_dt.date() and start_time <= local_time_of_day <= end_time:
                filtered_records.append(record)
                record_times.append(local_time)
                pulse_durations.append(pulse_duration)
                send_durations.append(send_duration)

    # Handle single-day or no-records edge cases
    if not filtered_records:
        summary_text = (
            f"<b>Query Results</b><br>"
            f"No production records found for the selected date range.<br>"
            f"<b>Skipped Records:</b> {skipped_records}<br>"
            f"<b>RFID Records:</b><br>"
            f"  Total Die RFID Scans: {len([r for r in rfid_records if r[2] == 'rfid_tag_die'])}<br>"
            f"  Total Operator RFID Scans: {len([r for r in rfid_records if r[2] == 'rfid_tag_people'])}<br>"
        )
        performance_data = {
            "total_count": 0,
            "production_rate": 0,
            "avg_time_per_product": 0,
            "total_die_scans": len([r for r in rfid_records if r[2] == "rfid_tag_die"]),
            "total_people_scans": len([r for r in rfid_records if r[2] == "rfid_tag_people"]),
            "total_anomalies": 0,
            "total_high_counts": 0,
            "total_large_intervals": 0
        }
        return filtered_records, intervals, record_times, pulse_durations, send_durations, anomalies, anomaly_records, large_interval_records, first_last_records, performance_data, summary_text

    # Calculate intervals and identify large intervals
    if len(record_times) > 1:
        intervals = [(record_times[i+1] - record_times[i]).total_seconds() / 60.0
                     for i in range(len(record_times)-1)]
        large_interval_records = [(i+1, filtered_records[i+1]) for i in range(len(intervals))
                                  if intervals[i] > LARGE_INTERVAL_THRESHOLD]

    # Detect anomalies in pulse_duration_ms using IQR method
    if pulse_durations:
        pulse_array = np.array(pulse_durations)
        q1 = np.percentile(pulse_array, 25)
        q3 = np.percentile(pulse_array, 75)
        iqr = q3 - q1
        lower_bound = q1 - 1.5 * iqr
        upper_bound = q3 + 1.5 * iqr
        anomalies = [i for i, pd in enumerate(pulse_durations) if pd < lower_bound or pd > upper_bound]
        # Add high count records to anomalies
        for idx, record in high_count_records:
            if idx < len(filtered_records) and idx not in anomalies:
                anomalies.append(idx)
        anomaly_records = [filtered_records[i] for i in anomalies if i < len(filtered_records)]

    # Process RFID records by type
    die_records = [r for r in rfid_records if r[2] == "rfid_tag_die"]
    people_records = [r for r in rfid_records if r[2] == "rfid_tag_people"]

    # Calculate summary statistics
    total_count = sum(count for _, count, _, _ in filtered_records)
    total_die_scans = len(die_records)
    total_people_scans = len(people_records)
    total_anomalies = len(anomalies)
    total_high_counts = len(high_count_records)
    total_large_intervals = len(large_interval_records)

    # Calculate working hours, handling single-day case
    time_diff_hours = 0
    for day in records_by_day:
        if records_by_day[day]:
            day_start = records_by_day[day][0][0].astimezone(IRAN_TZ)
            day_end = records_by_day[day][-1][0].astimezone(IRAN_TZ)
            day_hours = (day_end - day_start).total_seconds() / 3600
            # Ensure non-zero hours for single record
            time_diff_hours += max(day_hours, 1/3600)  # Minimum 1 second to avoid division by zero

    production_rate = total_count / time_diff_hours if time_diff_hours > 0 else 0
    avg_time_per_product = (time_diff_hours * 3600) / total_count if total_count > 0 else 0

    # Detailed summary text
    summary_text = (
        f"<b>Query Results</b><br>"
        f"<b>Total Production Count:</b> {total_count} (Note: count > 1 indicates delayed sends)<br>"
        f"<b>Production Rate:</b> {production_rate:.2f} products/hour<br>"
        f"<b>Average Time per Product:</b> {avg_time_per_product:.2f} seconds<br>"
        f"<b>Total Die RFID Scans:</b> {total_die_scans}<br>"
        f"<b>Total Operator RFID Scans:</b> {total_people_scans}<br>"
        f"<b>Total Anomalies (Pulse Duration or Count > 1):</b> {total_anomalies}<br>"
        f"<b>Records with Count > 1:</b> {total_high_counts}<br>"
        f"<b>Total Large Intervals (> {LARGE_INTERVAL_THRESHOLD} min):</b> {total_large_intervals}<br>"
        f"<b>Skipped Records:</b> {skipped_records}<br>"
        f"<b>Working Hours (Dynamic):</b><br>"
    )
    if len(records_by_day) == 1:
        day = list(records_by_day.keys())[0]
        if records_by_day[day]:
            start_time = records_by_day[day][0][0].astimezone(IRAN_TZ).strftime("%H:%M:%S")
            end_time = records_by_day[day][-1][0].astimezone(IRAN_TZ).strftime("%H:%M:%S")
            summary_text += f"  {day}: {start_time} to {end_time} (Single day)<br>"
    else:
        for day in sorted(records_by_day.keys()):
            if records_by_day[day]:
                start_time = records_by_day[day][0][0].astimezone(IRAN_TZ).strftime("%H:%M:%S")
                end_time = records_by_day[day][-1][0].astimezone(IRAN_TZ).strftime("%H:%M:%S")
                summary_text += f"  {day}: {start_time} to {end_time}<br>"

    performance_data = {
        "total_count": total_count,
        "production_rate": production_rate,
        "avg_time_per_product": avg_time_per_product,
        "total_die_scans": total_die_scans,
        "total_people_scans": total_people_scans,
        "total_anomalies": total_anomalies,
        "total_high_counts": total_high_counts,
        "total_large_intervals": total_large_intervals
    }

    return filtered_records, intervals, record_times, pulse_durations, send_durations, anomalies, anomaly_records, large_interval_records, first_last_records, performance_data, summary_text