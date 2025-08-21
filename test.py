import pandas as pd
from influxdb_client import InfluxDBClient
from datetime import datetime
import pytz

# InfluxDB configuration from defines.h
INFLUXDB_URL = "http://192.168.1.37:8086"
INFLUXDB_TOKEN = "_2B1s0cJGXznc9rXE41mxOusyn159HAAsMyGlLxiFkZd0V8dJKztKYVEPNeKGQSgc0gulh-vhnhn6ONcCr3lBA=="
INFLUXDB_ORG = "tooswasher"
INFLUXDB_BUCKET = "manufacture"

def find_working_sessions_with_pulse_count(start_time_str, die_id=None):
    """
    Retrieve working sessions and count total pulses for each session after a given time, 
    finding devices dynamically from records.
    
    Parameters:
    - start_time_str (str): Start time in ISO 8601 format (e.g., '2025-07-15T00:04:00Z')
    - die_id (str, optional): Specific die ID to filter sessions. If None, include all dies or records with no die.
    
    Returns:
    - List of dictionaries containing session details (device, personnel_id, die_id, start_time, end_time, duration, pulse_count)
    """
    try:
        # Initialize InfluxDB client
        client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
        query_api = client.query_api()

        # Parse start time and ensure UTC
        start_time = datetime.fromisoformat(start_time_str.replace('Z', '+00:00')).astimezone(pytz.UTC)
        
        # Step 1: Query personnel_activity to identify devices and sessions
        session_query = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
            |> range(start: {start_time.isoformat()})
            |> filter(fn: (r) => r._measurement == "personnel_activity")
            |> filter(fn: (r) => r._field == "relay_status" or r._field == "die_id" or r._field == "personnel_id")
            |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
            |> sort(columns: ["_time"])
        '''
        if die_id:
            session_query += f'''
            |> filter(fn: (r) => r.die_id == "{die_id}" or not exists r.die_id)
            '''
        # Execute session query
        session_tables = query_api.query(session_query)
        
        # Process session query results into a DataFrame
        session_records = []
        for table in session_tables:
            for record in table.records:
                session_records.append({
                    'time': record.get_time().astimezone(pytz.UTC),
                    'device': record.values.get('device', 'Unknown'),
                    'personnel_id': record.values.get('personnel_id', 'Unknown'),
                    'die_id': record.values.get('die_id', 'None'),
                    'relay_status': record.values.get('relay_status', None),
                })
        
        session_df = pd.DataFrame(session_records)
        if session_df.empty:
            print("No working sessions found after the specified time.")
            return []

        # Group sessions by device
        sessions_by_device = {}
        for device in session_df['device'].unique():
            device_df = session_df[session_df['device'] == device]
            sessions = []
            current_session = None
            
            for i, row in device_df.iterrows():
                if row['relay_status'] == 1 and current_session is None:
                    # Start of a new session
                    current_session = {
                        'device': row['device'],
                        'personnel_id': row['personnel_id'],
                        'die_id': row['die_id'],
                        'start_time': row['time'],
                    }
                elif row['relay_status'] == 0 and current_session is not None:
                    # End of a session, check if personnel_id and die_id match
                    if (row['personnel_id'] == current_session['personnel_id'] and
                        row['die_id'] == current_session['die_id']):
                        duration = (row['time'] - current_session['start_time']).total_seconds() / 60.0  # Duration in minutes
                        sessions.append({
                            'device': current_session['device'],
                            'personnel_id': current_session['personnel_id'],
                            'die_id': current_session['die_id'],
                            'start_time': current_session['start_time'],
                            'end_time': row['time'],
                            'duration_minutes': round(duration, 2),
                            'pulse_count': 0  # Initialize pulse_count, to be filled later
                        })
                        current_session = None
            
            sessions_by_device[device] = sessions

        if not any(sessions_by_device.values()):
            print("No complete working sessions found after the specified time.")
            return []

        # Step 2: Query production measurement to count pulses for each session
        for device, sessions in sessions_by_device.items():
            for session in sessions:
                start_time_str = session['start_time'].isoformat()
                end_time_str = session['end_time'].isoformat()
                
                # Build Flux query for production data within session time range
                production_query = f'''
                from(bucket: "{INFLUXDB_BUCKET}")
                    |> range(start: {start_time_str}, stop: {end_time_str})
                    |> filter(fn: (r) => r._measurement == "production")
                    |> filter(fn: (r) => r._field == "count")
                    |> filter(fn: (r) => r.device == "{device}")
                '''
                if session['die_id'] != 'None':
                    production_query += f'''
                    |> filter(fn: (r) => r.die_id == "{session['die_id']}" or not exists r.die_id)
                    '''
                # Execute production query
                production_tables = query_api.query(production_query)
                
                # Sum the count field to get total pulses
                total_pulses = 0
                for table in production_tables:
                    for record in table.records:
                        total_pulses += int(record.get_value())  # Sum the 'count' field

                session['pulse_count'] = total_pulses

        # Combine all sessions into a single list
        all_sessions = []
        for device, sessions in sessions_by_device.items():
            all_sessions.extend(sessions)
        # Format return data
        formatted_sessions = [
            {
                'device': session['device'],
                'personnel_id': session['personnel_id'],
                'die_id': session['die_id'],
                'start_time': session['start_time'].strftime('%Y-%m-%d %H:%M:%S %Z'),
                'end_time': session['end_time'].strftime('%Y-%m-%d %H:%M:%S %Z'),
                'duration_minutes': session['duration_minutes'],
                'pulse_count': session['pulse_count']
            }
            for session in all_sessions
        ]

        return formatted_sessions

    except Exception as e:
        print(f"Error retrieving working sessions or pulse counts: {str(e)}")
        return []
    finally:
        client.close()

# Example usage
if __name__ == "__main__":
    # Example: Get sessions and pulse counts for a specific die, finding devices dynamically
    sessions = find_working_sessions_with_pulse_count("2025-08-02T00:04:00Z", die_id=None)
    print(sessions)